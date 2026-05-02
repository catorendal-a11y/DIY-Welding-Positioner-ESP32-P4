#pragma once
// USB Mirror Protocol - Binary framing for local USB-C UI mirror

#include <cstddef>
#include <cstdint>

static constexpr uint16_t USB_MIRROR_WIDTH = 800;
static constexpr uint16_t USB_MIRROR_HEIGHT = 480;
static constexpr uint16_t USB_MIRROR_KEEPALIVE_TIMEOUT_MS = 1500;

static constexpr uint32_t USB_MIRROR_MAGIC = 0x31524D52u;  // "RMR1" little-endian
static constexpr uint8_t USB_MIRROR_VERSION = 1;
static constexpr size_t USB_MIRROR_HEADER_SIZE = 20;
static constexpr size_t USB_MIRROR_MAX_CHUNK_PAYLOAD = 4096;

enum UsbMirrorPacketType : uint8_t {
  USB_MIRROR_PACKET_HELLO = 1,
  USB_MIRROR_PACKET_KEEPALIVE = 2,
  USB_MIRROR_PACKET_VIDEO = 3,
  USB_MIRROR_PACKET_POINTER = 4,
  USB_MIRROR_PACKET_ARM = 5,
  USB_MIRROR_PACKET_STATS = 6,
  USB_MIRROR_PACKET_ERROR = 7,
};

enum UsbMirrorPointerState : uint8_t {
  USB_MIRROR_POINTER_RELEASED = 0,
  USB_MIRROR_POINTER_PRESSED = 1,
};

struct UsbMirrorHeader {
  uint32_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t flags;
  uint8_t reserved;
  uint32_t sequence;
  uint32_t payload_len;
  uint32_t payload_crc;
};

struct UsbMirrorVideoRect {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  uint16_t row;       // First row inside this rectangle chunk.
  uint16_t rows;      // Rows carried by this packet.
  uint8_t format;     // 1 = RGB565 little-endian.
  uint8_t reserved[3];
};

struct UsbMirrorPointer {
  int16_t x;
  int16_t y;
  uint8_t state;
  uint8_t reserved[3];
};

static constexpr uint8_t USB_MIRROR_FORMAT_RGB565_LE = 1;

inline uint16_t usb_mirror_clamp_u16(int32_t value, uint16_t maxValue) {
  if (value < 0) return 0;
  if (value > maxValue) return maxValue;
  return (uint16_t)value;
}

inline UsbMirrorPointer usb_mirror_clamp_pointer(int32_t x, int32_t y, uint8_t state) {
  UsbMirrorPointer p{};
  p.x = (int16_t)usb_mirror_clamp_u16(x, USB_MIRROR_WIDTH - 1);
  p.y = (int16_t)usb_mirror_clamp_u16(y, USB_MIRROR_HEIGHT - 1);
  p.state = (state == USB_MIRROR_POINTER_PRESSED) ? USB_MIRROR_POINTER_PRESSED
                                                  : USB_MIRROR_POINTER_RELEASED;
  return p;
}

inline bool usb_mirror_keepalive_fresh(uint32_t lastMs, uint32_t nowMs) {
  return (uint32_t)(nowMs - lastMs) < USB_MIRROR_KEEPALIVE_TIMEOUT_MS;
}

inline uint32_t usb_mirror_crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

inline void usb_mirror_put_u16(uint8_t* out, uint16_t value) {
  out[0] = (uint8_t)(value & 0xFFu);
  out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

inline void usb_mirror_put_u32(uint8_t* out, uint32_t value) {
  out[0] = (uint8_t)(value & 0xFFu);
  out[1] = (uint8_t)((value >> 8) & 0xFFu);
  out[2] = (uint8_t)((value >> 16) & 0xFFu);
  out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

inline uint16_t usb_mirror_get_u16(const uint8_t* in) {
  return (uint16_t)in[0] | ((uint16_t)in[1] << 8);
}

inline uint32_t usb_mirror_get_u32(const uint8_t* in) {
  return (uint32_t)in[0] |
         ((uint32_t)in[1] << 8) |
         ((uint32_t)in[2] << 16) |
         ((uint32_t)in[3] << 24);
}

inline bool usb_mirror_write_header(const UsbMirrorHeader& h, uint8_t* out, size_t outLen) {
  if (!out || outLen < USB_MIRROR_HEADER_SIZE) return false;

  usb_mirror_put_u32(out + 0, h.magic);
  out[4] = h.version;
  out[5] = h.type;
  out[6] = h.flags;
  out[7] = h.reserved;
  usb_mirror_put_u32(out + 8, h.sequence);
  usb_mirror_put_u32(out + 12, h.payload_len);
  usb_mirror_put_u32(out + 16, h.payload_crc);
  return true;
}

inline bool usb_mirror_read_header(const uint8_t* in, size_t inLen, UsbMirrorHeader& h) {
  if (!in || inLen < USB_MIRROR_HEADER_SIZE) return false;

  h.magic = usb_mirror_get_u32(in + 0);
  h.version = in[4];
  h.type = in[5];
  h.flags = in[6];
  h.reserved = in[7];
  h.sequence = usb_mirror_get_u32(in + 8);
  h.payload_len = usb_mirror_get_u32(in + 12);
  h.payload_crc = usb_mirror_get_u32(in + 16);

  if (h.magic != USB_MIRROR_MAGIC) return false;
  if (h.version != USB_MIRROR_VERSION) return false;
  return true;
}

inline bool usb_mirror_write_pointer(const UsbMirrorPointer& p, uint8_t* out, size_t outLen) {
  if (!out || outLen < sizeof(UsbMirrorPointer)) return false;

  usb_mirror_put_u16(out + 0, (uint16_t)p.x);
  usb_mirror_put_u16(out + 2, (uint16_t)p.y);
  out[4] = p.state;
  out[5] = 0;
  out[6] = 0;
  out[7] = 0;
  return true;
}

inline bool usb_mirror_read_pointer(const uint8_t* in, size_t inLen, UsbMirrorPointer& p) {
  if (!in || inLen < sizeof(UsbMirrorPointer)) return false;

  int16_t x = (int16_t)usb_mirror_get_u16(in + 0);
  int16_t y = (int16_t)usb_mirror_get_u16(in + 2);
  p = usb_mirror_clamp_pointer(x, y, in[4]);
  return true;
}
