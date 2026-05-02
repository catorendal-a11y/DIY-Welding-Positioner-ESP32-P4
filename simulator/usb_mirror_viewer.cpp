// USB Mirror Viewer - Windows SDL receiver for live ESP32 LVGL pixels

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <SDL2/SDL.h>
#include <windows.h>

#include "../src/mirror/usb_mirror_protocol.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static uint32_t now_ms() {
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  auto now = clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

static std::string win_error_string(DWORD err) {
  char* msg = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, err, 0, (LPSTR)&msg, 0, nullptr);
  std::string out = msg ? msg : "unknown error";
  if (msg) LocalFree(msg);
  return out;
}

class SerialPort {
public:
  ~SerialPort() {
    close();
  }

  bool open(const char* port, DWORD baud) {
    close();
    std::string path = port;
    if (path.rfind("\\\\.\\", 0) != 0) {
      path = "\\\\.\\" + path;
    }

    handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
      std::fprintf(stderr, "Open %s failed: %s\n", path.c_str(),
                   win_error_string(GetLastError()).c_str());
      return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
      std::fprintf(stderr, "GetCommState failed: %s\n", win_error_string(GetLastError()).c_str());
      close();
      return false;
    }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(handle_, &dcb)) {
      std::fprintf(stderr, "SetCommState failed: %s\n", win_error_string(GetLastError()).c_str());
      close();
      return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    SetCommTimeouts(handle_, &timeouts);
    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;
  }

  void close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

  bool valid() const {
    return handle_ != INVALID_HANDLE_VALUE;
  }

  int read(uint8_t* data, DWORD len) {
    if (!valid()) return -1;
    DWORD got = 0;
    if (!ReadFile(handle_, data, len, &got, nullptr)) {
      return -1;
    }
    return (int)got;
  }

  bool write_all(const uint8_t* data, DWORD len) {
    if (!valid()) return false;
    while (len > 0) {
      DWORD written = 0;
      if (!WriteFile(handle_, data, len, &written, nullptr)) {
        return false;
      }
      if (written == 0) {
        Sleep(1);
        continue;
      }
      data += written;
      len -= written;
    }
    return true;
  }

private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

struct Parser {
  uint8_t magicPos = 0;
  uint8_t headerPos = 0;
  uint32_t payloadPos = 0;
  uint8_t headerBuf[USB_MIRROR_HEADER_SIZE] = {};
  UsbMirrorHeader header{};
  std::vector<uint8_t> payload;
};

static void parser_reset(Parser& p) {
  p.magicPos = 0;
  p.headerPos = 0;
  p.payloadPos = 0;
  p.header = {};
  p.payload.clear();
}

static bool send_packet(SerialPort& serial, uint8_t type, const uint8_t* payload,
                        uint32_t payloadLen, uint32_t& sequence) {
  uint8_t headerBuf[USB_MIRROR_HEADER_SIZE];
  UsbMirrorHeader h{};
  h.magic = USB_MIRROR_MAGIC;
  h.version = USB_MIRROR_VERSION;
  h.type = type;
  h.sequence = sequence++;
  h.payload_len = payloadLen;
  h.payload_crc = payloadLen ? usb_mirror_crc32(payload, payloadLen) : 0;

  if (!usb_mirror_write_header(h, headerBuf, sizeof(headerBuf))) return false;
  if (!serial.write_all(headerBuf, sizeof(headerBuf))) return false;
  if (payloadLen > 0 && !serial.write_all(payload, payloadLen)) return false;
  return true;
}

static bool send_pointer(SerialPort& serial, int x, int y, bool pressed, uint32_t& sequence) {
  UsbMirrorPointer p = usb_mirror_clamp_pointer(x, y,
    pressed ? USB_MIRROR_POINTER_PRESSED : USB_MIRROR_POINTER_RELEASED);
  uint8_t payload[USB_MIRROR_POINTER_SIZE];
  usb_mirror_write_pointer(p, payload, sizeof(payload));
  return send_packet(serial, USB_MIRROR_PACKET_POINTER, payload, sizeof(payload), sequence);
}

static void apply_video_packet(const uint8_t* payload, uint32_t payloadLen,
                               std::vector<uint16_t>& framebuffer,
                               SDL_Texture* texture, uint32_t& lastVideoMs) {
  if (payloadLen < USB_MIRROR_VIDEO_RECT_SIZE) return;

  UsbMirrorVideoRect rect{};
  if (!usb_mirror_read_video_rect(payload, payloadLen, rect)) return;

  const uint8_t* pixels = payload + USB_MIRROR_VIDEO_RECT_SIZE;
  uint32_t pixelBytes = payloadLen - USB_MIRROR_VIDEO_RECT_SIZE;
  size_t pixelCount = (size_t)rect.w * rect.rows;
  uint32_t expected = (uint32_t)pixelCount * 2u;
  static std::vector<uint16_t> decodedPixels;
  const uint8_t* srcPixels = pixels;

  if (rect.format == USB_MIRROR_FORMAT_RGB565_LE) {
    if (pixelBytes != expected) return;
  } else if (rect.format == USB_MIRROR_FORMAT_RGB565_RLE) {
    if (decodedPixels.size() < pixelCount) {
      decodedPixels.resize(pixelCount);
    }
    if (!usb_mirror_decode_rgb565_rle(pixels, pixelBytes, decodedPixels.data(), pixelCount)) return;
    srcPixels = (const uint8_t*)decodedPixels.data();
  } else {
    return;
  }

  uint16_t dstY = (uint16_t)(rect.y + rect.row);
  if ((uint32_t)dstY + rect.rows > USB_MIRROR_HEIGHT) return;

  for (uint16_t r = 0; r < rect.rows; r++) {
    uint16_t* dst = framebuffer.data() + ((size_t)(dstY + r) * USB_MIRROR_WIDTH + rect.x);
    const uint8_t* src = srcPixels + ((size_t)r * rect.w * 2u);
    std::memcpy(dst, src, (size_t)rect.w * 2u);
  }

  SDL_Rect sdlRect{(int)rect.x, (int)dstY, (int)rect.w, (int)rect.rows};
  SDL_UpdateTexture(texture, &sdlRect,
                    framebuffer.data() + ((size_t)dstY * USB_MIRROR_WIDTH + rect.x),
                    USB_MIRROR_WIDTH * 2);
  lastVideoMs = now_ms();
}

static void handle_packet(const UsbMirrorHeader& h, const std::vector<uint8_t>& payload,
                          std::vector<uint16_t>& framebuffer, SDL_Texture* texture,
                          uint32_t& lastVideoMs, bool& helloSeen) {
  if (h.payload_len != payload.size()) return;
  if (h.payload_len > 0 && usb_mirror_crc32(payload.data(), payload.size()) != h.payload_crc) return;

  if (h.type == USB_MIRROR_PACKET_HELLO) {
    helloSeen = true;
    return;
  }

  if (h.type == USB_MIRROR_PACKET_VIDEO) {
    apply_video_packet(payload.data(), h.payload_len, framebuffer, texture, lastVideoMs);
  }
}

static void parse_bytes(Parser& p, const uint8_t* data, int len,
                        std::vector<uint16_t>& framebuffer, SDL_Texture* texture,
                        uint32_t& lastVideoMs, bool& helloSeen) {
  static const uint8_t magicBytes[4] = {'R', 'M', 'R', '1'};

  for (int i = 0; i < len; i++) {
    uint8_t b = data[i];

    if (p.headerPos == 0) {
      if (b == magicBytes[p.magicPos]) {
        p.headerBuf[p.magicPos] = b;
        p.magicPos++;
        if (p.magicPos == 4) {
          p.headerPos = 4;
        }
      } else {
        p.magicPos = (b == magicBytes[0]) ? 1 : 0;
        if (p.magicPos == 1) p.headerBuf[0] = b;
      }
      continue;
    }

    if (p.headerPos < USB_MIRROR_HEADER_SIZE) {
      p.headerBuf[p.headerPos++] = b;
      if (p.headerPos == USB_MIRROR_HEADER_SIZE) {
        if (!usb_mirror_read_header(p.headerBuf, sizeof(p.headerBuf), p.header) ||
            p.header.payload_len > (USB_MIRROR_VIDEO_RECT_SIZE + USB_MIRROR_MAX_CHUNK_PAYLOAD)) {
          parser_reset(p);
          continue;
        }
        p.payload.assign(p.header.payload_len, 0);
        p.payloadPos = 0;
        if (p.header.payload_len == 0) {
          handle_packet(p.header, p.payload, framebuffer, texture, lastVideoMs, helloSeen);
          parser_reset(p);
        }
      }
      continue;
    }

    if (p.payloadPos < p.payload.size()) {
      p.payload[p.payloadPos++] = b;
      if (p.payloadPos == p.payload.size()) {
        handle_packet(p.header, p.payload, framebuffer, texture, lastVideoMs, helloSeen);
        parser_reset(p);
      }
    }
  }
}

static int map_coord(int value, int windowSize, int logicalSize) {
  if (windowSize <= 0) return 0;
  int mapped = value * logicalSize / windowSize;
  return std::max(0, std::min(logicalSize - 1, mapped));
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: rotator_usb_mirror.exe COM5 [baud]\n");
    return 2;
  }

  const char* port = argv[1];
  DWORD baud = (argc >= 3) ? (DWORD)std::strtoul(argv[2], nullptr, 10) : USB_MIRROR_DEFAULT_BAUD;

  SerialPort serial;
  if (!serial.open(port, baud)) return 1;

  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("Rotator USB Mirror", SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED, USB_MIRROR_WIDTH, USB_MIRROR_HEIGHT, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
    SDL_TEXTUREACCESS_STREAMING, USB_MIRROR_WIDTH, USB_MIRROR_HEIGHT);

  if (!window || !renderer || !texture) {
    std::fprintf(stderr, "SDL window setup failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  std::vector<uint16_t> framebuffer((size_t)USB_MIRROR_WIDTH * USB_MIRROR_HEIGHT, 0);
  SDL_UpdateTexture(texture, nullptr, framebuffer.data(), USB_MIRROR_WIDTH * 2);

  uint32_t txSequence = 1;
  uint32_t lastHelloMs = 0;
  uint32_t lastKeepaliveMs = 0;
  uint32_t lastTitleMs = 0;
  uint32_t lastVideoMs = 0;
  bool helloSeen = false;
  bool mouseDown = false;
  Parser parser;

  bool running = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running = false;
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        mouseDown = true;
        send_pointer(serial, map_coord(ev.button.x, ww, USB_MIRROR_WIDTH),
                     map_coord(ev.button.y, wh, USB_MIRROR_HEIGHT), true, txSequence);
      } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        mouseDown = false;
        send_pointer(serial, map_coord(ev.button.x, ww, USB_MIRROR_WIDTH),
                     map_coord(ev.button.y, wh, USB_MIRROR_HEIGHT), false, txSequence);
      } else if (ev.type == SDL_MOUSEMOTION && mouseDown) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window, &ww, &wh);
        send_pointer(serial, map_coord(ev.motion.x, ww, USB_MIRROR_WIDTH),
                     map_coord(ev.motion.y, wh, USB_MIRROR_HEIGHT), true, txSequence);
      }
    }

    uint32_t now = now_ms();
    if (!helloSeen && now - lastHelloMs >= 500) {
      send_packet(serial, USB_MIRROR_PACKET_HELLO, nullptr, 0, txSequence);
      lastHelloMs = now;
    }
    if (now - lastKeepaliveMs >= 400) {
      send_packet(serial, USB_MIRROR_PACKET_KEEPALIVE, nullptr, 0, txSequence);
      lastKeepaliveMs = now;
    }

    uint8_t rx[8192];
    int got = serial.read(rx, sizeof(rx));
    if (got < 0) {
      std::fprintf(stderr, "Serial read failed\n");
      running = false;
    } else if (got > 0) {
      parse_bytes(parser, rx, got, framebuffer, texture, lastVideoMs, helloSeen);
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    if (now - lastTitleMs >= 500) {
      char title[160];
      const char* state = (lastVideoMs != 0 && now - lastVideoMs < 1500) ? "LIVE" : "WAITING";
      std::snprintf(title, sizeof(title), "Rotator USB Mirror - %s @ %lu - %s",
                    port, (unsigned long)baud, state);
      SDL_SetWindowTitle(window, title);
      lastTitleMs = now;
    }

    SDL_Delay(1);
  }

  uint8_t disarm = 0;
  send_packet(serial, USB_MIRROR_PACKET_ARM, &disarm, 1, txSequence);
  send_pointer(serial, 0, 0, false, txSequence);

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
