#pragma once
#include <cmath>
#include <cstdlib>

// Parse a complete finite number, accepting either decimal separator.
inline bool parse_float_entry(const char* text, float& value) {
  if (!text || !*text) return false;
  char entry[24]; unsigned n = 0;
  for (; text[n] && n < sizeof(entry)-1; ++n) entry[n] = text[n] == ',' ? '.' : text[n];
  if (text[n]) return false;
  entry[n] = 0; char* end = nullptr; value = std::strtof(entry, &end);
  return end != entry && *end == 0 && std::isfinite(value);
}
