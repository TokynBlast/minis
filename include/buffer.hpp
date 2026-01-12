#pragma once
#include <string>
#include <iostream>

#define MAX_CHARS 4096

namespace minis {
  struct OutBuffer {
    static constexpr size_t BUF_SIZE = MAX_CHARS;
    std::string buf;

    void write(const char* s) {
      buf += s;
      if (buf.size() >= BUF_SIZE) flush();
    }

    void flush() {
      if (!buf.empty()) {
        std::cout << buf;
        std::cout.flush();
        buf.clear();
      }
    }

    ~OutBuffer() { flush(); }
  };

  inline static OutBuffer screen;
}
