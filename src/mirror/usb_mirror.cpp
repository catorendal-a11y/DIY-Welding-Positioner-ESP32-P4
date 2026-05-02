// USB Mirror - Non-blocking USB-C LVGL pixel mirror and remote pointer input

#include "usb_mirror.h"

#include "../app_state.h"
#include "../config.h"

#include <Arduino.h>
#include <atomic>
#include <cstring>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#if ENABLE_USB_UI_MIRROR

static constexpr uint8_t MIRROR_CHUNK_COUNT = 32;
static constexpr size_t MIRROR_RX_PAYLOAD_MAX = 64;

struct MirrorChunk {
  UsbMirrorVideoRect rect;
  uint16_t pixelBytes;
  uint8_t pixels[USB_MIRROR_MAX_CHUNK_PAYLOAD];
};

static MirrorChunk* g_chunks = nullptr;
static QueueHandle_t g_freeQueue = nullptr;
static QueueHandle_t g_readyQueue = nullptr;

static std::atomic<bool> g_ready{false};
static std::atomic<bool> g_connected{false};
static std::atomic<bool> g_armed{false};
static std::atomic<uint32_t> g_lastKeepaliveMs{0};
static std::atomic<uint32_t> g_sequence{1};
static std::atomic<uint32_t> g_dropCount{0};

static std::atomic<int> g_pointerX{0};
static std::atomic<int> g_pointerY{0};
static std::atomic<int> g_pointerState{USB_MIRROR_POINTER_RELEASED};

static void pointer_release() {
  g_pointerState.store(USB_MIRROR_POINTER_RELEASED, std::memory_order_release);
}

static bool keepalive_fresh_now() {
  uint32_t last = g_lastKeepaliveMs.load(std::memory_order_acquire);
  if (last == 0) return false;
  return usb_mirror_keepalive_fresh(last, millis());
}

static bool serial_write_all(const uint8_t* data, size_t len) {
  if (!data && len > 0) return false;
  while (len > 0) {
    size_t written = Serial.write(data, len);
    if (written == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    data += written;
    len -= written;
  }
  return true;
}

static bool send_packet(uint8_t type, const uint8_t* payload, uint32_t payloadLen) {
  uint8_t headerBuf[USB_MIRROR_HEADER_SIZE];
  UsbMirrorHeader h{};
  h.magic = USB_MIRROR_MAGIC;
  h.version = USB_MIRROR_VERSION;
  h.type = type;
  h.sequence = g_sequence.fetch_add(1, std::memory_order_acq_rel);
  h.payload_len = payloadLen;
  h.payload_crc = payloadLen ? usb_mirror_crc32(payload, payloadLen) : 0;

  if (!usb_mirror_write_header(h, headerBuf, sizeof(headerBuf))) return false;
  if (!serial_write_all(headerBuf, sizeof(headerBuf))) return false;
  if (payloadLen > 0 && !serial_write_all(payload, payloadLen)) return false;
  return true;
}

static bool send_video_chunk(const MirrorChunk& chunk) {
  uint8_t headerBuf[USB_MIRROR_HEADER_SIZE];
  uint8_t rectBuf[USB_MIRROR_VIDEO_RECT_SIZE];
  if (!usb_mirror_write_video_rect(chunk.rect, rectBuf, sizeof(rectBuf))) return false;

  uint32_t crc = usb_mirror_crc32_begin();
  crc = usb_mirror_crc32_update(crc, rectBuf, sizeof(rectBuf));
  crc = usb_mirror_crc32_update(crc, chunk.pixels, chunk.pixelBytes);
  crc = usb_mirror_crc32_finish(crc);

  UsbMirrorHeader h{};
  h.magic = USB_MIRROR_MAGIC;
  h.version = USB_MIRROR_VERSION;
  h.type = USB_MIRROR_PACKET_VIDEO;
  h.sequence = g_sequence.fetch_add(1, std::memory_order_acq_rel);
  h.payload_len = (uint32_t)(sizeof(rectBuf) + chunk.pixelBytes);
  h.payload_crc = crc;

  if (!usb_mirror_write_header(h, headerBuf, sizeof(headerBuf))) return false;
  if (!serial_write_all(headerBuf, sizeof(headerBuf))) return false;
  if (!serial_write_all(rectBuf, sizeof(rectBuf))) return false;
  if (!serial_write_all(chunk.pixels, chunk.pixelBytes)) return false;
  return true;
}

static void free_chunk(uint8_t index) {
  if (g_freeQueue) {
    xQueueSend(g_freeQueue, &index, 0);
  }
}

static void drain_ready_queue_once() {
  if (!g_readyQueue || !g_freeQueue) return;

  uint8_t index = 0;
  if (xQueueReceive(g_readyQueue, &index, 0) != pdTRUE) return;

  if (g_connected.load(std::memory_order_acquire) && keepalive_fresh_now()) {
    send_video_chunk(g_chunks[index]);
  }
  free_chunk(index);
}

static void handle_packet(const UsbMirrorHeader& h, const uint8_t* payload) {
  uint32_t now = millis();

  if (h.type == USB_MIRROR_PACKET_HELLO) {
    g_connected.store(true, std::memory_order_release);
    g_lastKeepaliveMs.store(now, std::memory_order_release);
    pointer_release();
    send_packet(USB_MIRROR_PACKET_HELLO, nullptr, 0);
    g_screenRedraw.store(true, std::memory_order_release);
    return;
  }

  if (h.type == USB_MIRROR_PACKET_KEEPALIVE) {
    g_connected.store(true, std::memory_order_release);
    g_lastKeepaliveMs.store(now, std::memory_order_release);
    return;
  }

  if (h.type == USB_MIRROR_PACKET_POINTER) {
    g_connected.store(true, std::memory_order_release);
    g_lastKeepaliveMs.store(now, std::memory_order_release);

    UsbMirrorPointer p{};
    if (!usb_mirror_read_pointer(payload, h.payload_len, p)) {
      pointer_release();
      return;
    }

    if (!g_armed.load(std::memory_order_acquire)) {
      pointer_release();
      return;
    }

    g_pointerX.store(p.x, std::memory_order_release);
    g_pointerY.store(p.y, std::memory_order_release);
    g_pointerState.store(p.state, std::memory_order_release);
    return;
  }

  if (h.type == USB_MIRROR_PACKET_ARM && h.payload_len >= 1 && payload[0] == 0) {
    g_armed.store(false, std::memory_order_release);
    pointer_release();
  }
}

static void parser_reset(uint8_t& magicPos, uint8_t& headerPos, uint8_t& payloadPos,
                         uint32_t& payloadLen, UsbMirrorHeader& header) {
  magicPos = 0;
  headerPos = 0;
  payloadPos = 0;
  payloadLen = 0;
  header = {};
}

static void parse_serial_input() {
  static const uint8_t magicBytes[4] = {'R', 'M', 'R', '1'};
  static uint8_t magicPos = 0;
  static uint8_t headerPos = 0;
  static uint8_t payloadPos = 0;
  static uint8_t headerBuf[USB_MIRROR_HEADER_SIZE];
  static uint8_t payloadBuf[MIRROR_RX_PAYLOAD_MAX];
  static uint32_t payloadLen = 0;
  static UsbMirrorHeader header{};

  while (Serial.available() > 0) {
    int raw = Serial.read();
    if (raw < 0) return;
    uint8_t b = (uint8_t)raw;

    if (headerPos == 0) {
      if (b == magicBytes[magicPos]) {
        headerBuf[magicPos] = b;
        magicPos++;
        if (magicPos == 4) {
          headerPos = 4;
        }
      } else {
        magicPos = (b == magicBytes[0]) ? 1 : 0;
        if (magicPos == 1) headerBuf[0] = b;
      }
      continue;
    }

    if (headerPos < USB_MIRROR_HEADER_SIZE) {
      headerBuf[headerPos++] = b;
      if (headerPos == USB_MIRROR_HEADER_SIZE) {
        if (!usb_mirror_read_header(headerBuf, sizeof(headerBuf), header) ||
            header.payload_len > MIRROR_RX_PAYLOAD_MAX) {
          parser_reset(magicPos, headerPos, payloadPos, payloadLen, header);
          continue;
        }

        payloadLen = header.payload_len;
        payloadPos = 0;
        if (payloadLen == 0) {
          handle_packet(header, payloadBuf);
          parser_reset(magicPos, headerPos, payloadPos, payloadLen, header);
        }
      }
      continue;
    }

    if (payloadPos < payloadLen) {
      payloadBuf[payloadPos++] = b;
      if (payloadPos == payloadLen) {
        uint32_t crc = usb_mirror_crc32(payloadBuf, payloadLen);
        if (crc == header.payload_crc) {
          handle_packet(header, payloadBuf);
        } else {
          pointer_release();
        }
        parser_reset(magicPos, headerPos, payloadPos, payloadLen, header);
      }
    }
  }
}

void usb_mirror_begin() {
  if (g_ready.load(std::memory_order_acquire)) return;

  g_chunks = (MirrorChunk*)heap_caps_calloc(MIRROR_CHUNK_COUNT, sizeof(MirrorChunk),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_freeQueue = xQueueCreate(MIRROR_CHUNK_COUNT, sizeof(uint8_t));
  g_readyQueue = xQueueCreate(MIRROR_CHUNK_COUNT, sizeof(uint8_t));

  if (!g_chunks || !g_freeQueue || !g_readyQueue) {
    LOG_E("USB mirror init failed");
    g_chunks = nullptr;
    g_freeQueue = nullptr;
    g_readyQueue = nullptr;
    return;
  }

  for (uint8_t i = 0; i < MIRROR_CHUNK_COUNT; i++) {
    xQueueSend(g_freeQueue, &i, 0);
  }

  g_ready.store(true, std::memory_order_release);
  LOG_I("USB mirror ready: %u chunks x %u bytes", MIRROR_CHUNK_COUNT,
        (unsigned)USB_MIRROR_MAX_CHUNK_PAYLOAD);
}

void usbMirrorTask(void* pvParameters) {
  (void)pvParameters;

  for (;;) {
    parse_serial_input();

    if (g_connected.load(std::memory_order_acquire) && !keepalive_fresh_now()) {
      g_connected.store(false, std::memory_order_release);
      g_armed.store(false, std::memory_order_release);
      pointer_release();
    }

    drain_ready_queue_once();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool usb_mirror_enqueue_rect(const lv_area_t* area, const uint8_t* px_map) {
  if (!g_ready.load(std::memory_order_acquire) || !area || !px_map) return false;
  if (!g_connected.load(std::memory_order_acquire) || !keepalive_fresh_now()) return false;
  if (!g_freeQueue || !g_readyQueue || !g_chunks) return false;

  int32_t x1 = area->x1;
  int32_t y1 = area->y1;
  int32_t x2 = area->x2;
  int32_t y2 = area->y2;

  if (x1 < 0 || y1 < 0 || x2 >= USB_MIRROR_WIDTH || y2 >= USB_MIRROR_HEIGHT) return false;
  if (x2 < x1 || y2 < y1) return false;

  uint16_t w = (uint16_t)(x2 - x1 + 1);
  uint16_t h = (uint16_t)(y2 - y1 + 1);
  uint16_t maxRows = (uint16_t)(USB_MIRROR_MAX_CHUNK_PAYLOAD / ((size_t)w * 2u));
  if (maxRows == 0) return false;
  if (maxRows > USB_MIRROR_CHUNK_ROWS) maxRows = USB_MIRROR_CHUNK_ROWS;

  bool queuedAny = false;
  for (uint16_t row = 0; row < h; row += maxRows) {
    uint16_t rows = (uint16_t)((row + maxRows <= h) ? maxRows : (h - row));
    uint32_t pixelBytes = (uint32_t)w * rows * 2u;

    uint8_t index = 0;
    if (xQueueReceive(g_freeQueue, &index, 0) != pdTRUE) {
      g_dropCount.fetch_add(1, std::memory_order_acq_rel);
      return queuedAny;
    }

    MirrorChunk& chunk = g_chunks[index];
    chunk.rect = {};
    chunk.rect.x = (uint16_t)x1;
    chunk.rect.y = (uint16_t)y1;
    chunk.rect.w = w;
    chunk.rect.h = h;
    chunk.rect.row = row;
    chunk.rect.rows = rows;
    chunk.rect.format = USB_MIRROR_FORMAT_RGB565_LE;
    chunk.pixelBytes = (uint16_t)pixelBytes;

    const uint8_t* src = px_map + ((size_t)row * w * 2u);
    memcpy(chunk.pixels, src, pixelBytes);

    if (xQueueSend(g_readyQueue, &index, 0) != pdTRUE) {
      free_chunk(index);
      g_dropCount.fetch_add(1, std::memory_order_acq_rel);
      return queuedAny;
    }
    queuedAny = true;
  }

  return queuedAny;
}

void usb_mirror_read_pointer(lv_indev_data_t* data) {
  if (!data) return;

  if (!g_armed.load(std::memory_order_acquire) || !g_connected.load(std::memory_order_acquire) ||
      !keepalive_fresh_now()) {
    data->state = LV_INDEV_STATE_RELEASED;
    pointer_release();
    return;
  }

  data->point.x = (lv_coord_t)g_pointerX.load(std::memory_order_acquire);
  data->point.y = (lv_coord_t)g_pointerY.load(std::memory_order_acquire);
  data->state = (g_pointerState.load(std::memory_order_acquire) == USB_MIRROR_POINTER_PRESSED)
                  ? LV_INDEV_STATE_PRESSED
                  : LV_INDEV_STATE_RELEASED;
}

bool usb_mirror_is_armed() {
  return g_armed.load(std::memory_order_acquire);
}

void usb_mirror_set_armed(bool armed) {
  g_armed.store(armed, std::memory_order_release);
  if (!armed) pointer_release();
}

bool usb_mirror_is_connected() {
  return g_connected.load(std::memory_order_acquire) && keepalive_fresh_now();
}

uint32_t usb_mirror_drop_count() {
  return g_dropCount.load(std::memory_order_acquire);
}

#else

void usb_mirror_begin() {}
void usbMirrorTask(void*) {}

bool usb_mirror_enqueue_rect(const lv_area_t*, const uint8_t*) {
  return false;
}

void usb_mirror_read_pointer(lv_indev_data_t* data) {
  if (data) data->state = LV_INDEV_STATE_RELEASED;
}

bool usb_mirror_is_armed() {
  return false;
}

void usb_mirror_set_armed(bool) {}

bool usb_mirror_is_connected() {
  return false;
}

uint32_t usb_mirror_drop_count() {
  return 0;
}

#endif
