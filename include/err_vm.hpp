#pragma once
#include <cstdlib>
// FiXME: Should use Minis buffer instead
#include "buffer.hpp"
#include <string>

namespace minis {
  // VM error: always fatal, no location
  inline void fatal(std::string msg) {
    screen.write("FATAL ERROR: " + msg + "\n");
    std::exit(1);
  }
  #define VM_ERR(MSG) ::minis::vm_fatal(MSG)
}
