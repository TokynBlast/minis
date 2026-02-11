
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <print>
#include <string>
#include <vector>
#include <cstdio>

extern "C" {

  void print(const char* text)
  {
    if (!text) return;
    std::fputs(text, stdout);
  }

}
