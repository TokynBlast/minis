#pragma once
#include "sso.hpp"
#include "token.hpp"
#include <vector>

namespace lang {
  struct Source;

  class Compiler {
  public:
    explicit Compiler(const std::vector<Token>& tokens);

    void compileToFile(const CString& out);
  };
}