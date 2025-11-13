#pragma once
#include <vector>
#include "sso.hpp"
#include "token.hpp"
#include "types.hpp"
#include "value.hpp"

namespace lang {
  void CompileToFile(const CString& srcName, const CString& srcText, const CString& outPath);
}