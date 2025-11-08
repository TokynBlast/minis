#pragma once
// GENERAL: Is this even needed..?
#include <vector>
#include <unordered_set>
#include "diagnose.hpp"
#include "err.hpp"

namespace lang {
  struct Context {
    std::vector<Diagnostic> diags;
    const Source* src = nullptr;
    std::vector<size_t> posmap;
  };

  inline Context& ctx();

  bool HasErrors();

  inline bool IsIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
  inline bool IsIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

  static bool is_builtin(const std::string& s){
    static const std::unordered_set<std::string> bi={ "print","abs","neg","range","len","input","max","min","sort","reverse","sum" };
    return bi.count(s)!=0;
  }
  
  void diag(DiagKind kind, size_t beg, size_t end, std::string msg);
}