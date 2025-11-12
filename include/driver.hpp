#pragma once
#include "sso.hpp"

namespace lang {
  CString ReadFile(const CString& path);

  void CompileToFile(const CString& srcName,
                     const CString& srcText,
                     const CString& out);

  void run(const CString& path);
}
