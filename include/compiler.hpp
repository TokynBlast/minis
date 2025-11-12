#pragma once
#include "sso.hpp"

namespace lang {
  struct Source;

  class Compiler {
  public:
    explicit Compiler(const CString& src);
    void compileToFile(const CString* out);
  };
}