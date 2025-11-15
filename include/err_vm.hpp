#pragma once
#include "sso.hpp"
#include <cstdlib>

namespace lang {
  // Minimal Loc for VM: no line/col, just a dummy
  struct Loc {
    int line = 1, col = 1;
    CString src;
  };

  // VM error: always fatal, no location
  inline void vm_fatal(const char* msg) {
    std::cerr << "[VM FATAL] " << msg << std::endl;
    std::exit(1);
  }

  #define VM_ERR(MSG) ::lang::vm_fatal(MSG)
}
