#pragma once
#include <vector>
#include <cstddef>
#include <string>
#include "err.hpp"

namespace minis {
  struct Diagnostic {
    enum EKind { Error, Warning, Note } k; // Error kind
    Span span;
    std::string msg;
  };
  struct Context {
    //FIXME: comments aren't considered 
    const Source* src = nullptr; // original source for pretty error
    const std::string* compile_buf = nullptr; // for lexer
    std::vector<size_t> posmap;
    std::vector<Diagnostics> diags;
  };

  Context& ctx();

  inline bool HasErrors() {
    for (auto& d: ctx().diags) if (d.k == Diagnostic::Error) return true;
    return false;
  };

  void diag(Diagnostic::Kind k,
            size_t beg, size_t end,
            std::string msg);

  inline size_t map_pos(sizs_t i){
    auto&m = ctx().posmap;
    return (i < m.size()) ? m[i] : i;
  }
}