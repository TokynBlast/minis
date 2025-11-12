#pragma once
// GENERAL: Is this even needed..?
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cctype>
#include "err.hpp"
#include "token.hpp"
#include "sso.hpp"

namespace lang {
  struct Context {
    // current source
    const Source* src = nullptr;
    // last produced token stream
    std::vector<Token> tokens;
    std::vector<size_t> posmap;
  };

  Context& ctx();

  bool HasErrors();


  inline bool IsIdStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
  }

  inline bool IsIdCont(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
  }

  static bool is_builtin(const char* s) {
    static const std::unordered_set<const char*> bi = {
      "print", "abs", "neg", "range", "len", "input",
      "max", "min", "sort", "reverse", "sum"
    };
    return bi.count(s) != 0;
  }

  // Overload for CString convenience
  inline bool is_builtin(const CString& s) {
    return is_builtin(s.c_str());
  }
}