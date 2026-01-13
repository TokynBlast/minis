#pragma once
#include <cstdlib>
#include <print>

namespace minis {
  // VM error: always fatal, no location
  inline void fatal(const char* msg) {
    std::print("FATAL ERROR: {}\n");
    std::exit(1);
  }
  #define ERR(MSG) ::minis::vm_fatal(MSG)
}
