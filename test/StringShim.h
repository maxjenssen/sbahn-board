#pragma once
#include <string>
#include <ctime>

// Minimal Arduino-String stand-in for host tests. Only the surface
// BoardLogic.h uses: construction, +, ==, replace, indexOf, length, c_str.
class String {
  std::string s;
public:
  String() {}
  String(const char *c) : s(c) {}
  String(const std::string &x) : s(x) {}
  String(int v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  void replace(const char *from, const char *to) {
    std::string f(from), t(to);
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) {
      s.replace(pos, f.size(), t);
      pos += t.size();
    }
  }
  unsigned int length() const { return (unsigned int)s.size(); }
  const char *c_str() const { return s.c_str(); }
  char operator[](unsigned int i) const { return s[i]; }
  String operator+(const String &o) const { return String(s + o.s); }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator==(const char *o) const { return s == o; }
  int indexOf(const char *sub) const {
    size_t p = s.find(sub);
    return p == std::string::npos ? -1 : (int)p;
  }
};
inline String operator+(const char *a, const String &b) { return String(a) + b; }
