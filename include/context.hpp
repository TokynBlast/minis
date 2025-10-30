#pragma once
#include <vector>
#include <cstddef>
#include <string>
#include "err.hpp"
#include "diagnose.hpp"

namespace minis {
  struct Context {
    //FIXME: comments aren't considered 
    const Source* src = nullptr; // original source for pretty error
    const std::string* compile_buf = nullptr; // for lexer
    std::vector<size_t> posmap;
    std::vector<Diagnostic> diags;
  };

  Context& ctx();

  inline bool HasErrors() {
    for (auto& d: ctx().diags) if (d.k == Diagnostic::Error) return true;
    return false;
  };

  void diag(Diagnostic::EKind k,
            size_t beg, size_t end,
            std::string msg);

  inline size_t map_pos(size_t i){
    auto&m = ctx().posmap;
    return (i < m.size()) ? m[i] : i;
  }
}