#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include "err.hpp"

namespace minis {
  struct Diagnostic {
    enum EKind { Error, Warning, Note };
    std::size_t beg;
    std::size_t end;
    std::string msg;
    Span span;
  };

  extern std::vector<Diagnostic> g_diags;

  void DIAG(Diagnostic::EKind k, size_t beg, size_t end, std::string msg);
  bool hasErrors();
}
