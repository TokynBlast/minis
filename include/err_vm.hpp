#pragma once
#include "sso.hpp"
#include <cstdlib>
// FiXME: Should use Minis buffer instead
#include <iostream>

namespace minis {
  // VM error: always fatal, no location
  inline void fatal(const char* msg) {
    // FIXME: Should use Minis buffer instead
    std::cerr << "[FATAL ERROR]: " << msg << '\n';
    std::exit(1);
  }

  #define VM_ERR(MSG) ::minis::vm_fatal(MSG)
}
