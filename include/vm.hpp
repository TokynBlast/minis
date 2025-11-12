#pragma once
#include "sso.hpp"

namespace lang {
  class VM {
  public:
    VM() = default;
    // load bytecode file (path)
    void load(const CString& path);
    // run the loaded bytecode
    void run();
  };
}