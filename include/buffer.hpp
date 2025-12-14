#pragma once
#include <cstdio>
#include <cstring>

namespace minis {
  struct OutBuffer {
    static constexpr size_t BUF_SIZE = 4096;
    char buf[BUF_SIZE];
    size_t pos = 0;

    void write(const char* s) {
      size_t len = std::strlen(s);
      if (pos + len >= BUF_SIZE) flush();
      if (len >= BUF_SIZE) {
        fwrite(s, 1, len, stdout);
      } else {
        std::memcpy(buf + pos, s, len);
        pos += len;
      }
    }

    void flush() {
      if (pos > 0) {
        fwrite(buf, 1, pos, stdout);
        fflush(stdout);
        pos = 0;
      }
    }

    ~OutBuffer() { flush(); }
  };

  inline static OutBuffer screen;
}
