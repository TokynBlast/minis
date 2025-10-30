#pragma once
#include <string>
#include <vector>
#include "context.hpp"

namespace minis {
  struct Diagnostic {
    enum Kind { Error, Warning, Note } k;
    Span span; std::string msg;
  };

  extern std::vector<Diagnostic> g_diags;

  void DIAG(Diagnostic::Kind k, size_t beg, size_t end, std::string m);
  bool hasErrors();
}
