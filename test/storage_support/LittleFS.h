#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#define FILE_READ "r"
inline std::map<std::string,std::vector<uint8_t>> testLegacy;
class File {
 std::vector<uint8_t> bytes; bool opened;
 public:
 File(const char* path):bytes(testLegacy[path]),opened(testLegacy.count(path)>0) {}
 explicit operator bool() const { return opened; }
 size_t size() const { return bytes.size(); }
 size_t read(uint8_t* out,size_t size) { size=std::min(size,bytes.size()); memcpy(out,bytes.data(),size); return size; }
 void close() {}
};
struct TestLittleFS {
 bool begin(bool) { return !testLegacy.empty(); }
 bool exists(const char* path) { return testLegacy.count(path)>0; }
 File open(const char* path,const char*) { return File(path); }
 void end() {}
};
inline TestLittleFS LittleFS;
