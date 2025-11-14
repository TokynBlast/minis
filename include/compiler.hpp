#pragma once
#include <vector>
#include "sso.hpp"
#include "token.hpp"

namespace lang {
  void CompileToFileImpl(const std::vector<Token>& tokens, const CString& outPath);
}