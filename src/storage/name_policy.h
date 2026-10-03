#pragma once
#include <cstddef>
#include <cstring>
inline bool program_name_valid(const char* text, size_t capacity, bool ascii_only = false) {
  if (!text || !capacity) return false;
  size_t i = 0;
  while (i < capacity && text[i]) {
    const unsigned char c = static_cast<unsigned char>(text[i++]);
    if (c < 0x20 || c == 0x7f) return false;
    if (c < 0x80) continue;
    if (ascii_only) return false;
    unsigned count = 0; unsigned code = 0, minimum = 0;
    if (c >= 0xc2 && c <= 0xdf) { count=1; code=c & 0x1f; minimum=0x80; }
    else if (c >= 0xe0 && c <= 0xef) { count=2; code=c & 0x0f; minimum=0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { count=3; code=c & 7; minimum=0x10000; }
    else return false;
    while (count--) {
      if (i >= capacity) return false;
      const unsigned char next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 0x3f);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
  }
  return i > 0 && i < capacity && text[i] == 0;
}
