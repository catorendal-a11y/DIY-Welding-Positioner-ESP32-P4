#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
inline std::map<std::string,std::vector<uint8_t>> testNvs;
inline bool testNvsShortWrite=false, testNvsShortRead=false, testNvsEraseFailure=false;
inline void (*testNvsBeforeClear)() = nullptr;
class Preferences {
 public:
  bool begin(const char*,bool) { return true; }
  size_t getBytesLength(const char* key) { return testNvs[key].size(); }
  size_t getBytes(const char* key,void* out,size_t length) {
    const size_t size=std::min(length,testNvs[key].size());
    if (size) memcpy(out,testNvs[key].data(),size);
    return testNvsShortRead && size ? size-1 : size;
  }
  size_t putBytes(const char* key,const void* data,size_t length) {
    if (testNvsShortWrite) return length ? length-1 : 0;
    const auto bytes=static_cast<const uint8_t*>(data); testNvs[key]={bytes,bytes+length}; return length;
  }
  bool remove(const char* key) { return testNvs.erase(key)>0; }
  bool clear() { if (testNvsBeforeClear) testNvsBeforeClear(); if(testNvsEraseFailure) return false; testNvs.clear(); return true; }
};
