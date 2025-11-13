#pragma once
#include <vector>
#include "sso.hpp"
#include "token.hpp"
#include "types.hpp"
#include "value.hpp"

namespace lang {
  struct Compiler;
  void CompileToFile(const CString& srcName, const CString& srcText, const CString& outPath);
  struct CompilerInterface {
    static void compile(const std::vector<Token>& tokens, const CString& outPath);
  };
}