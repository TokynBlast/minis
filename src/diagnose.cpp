#include <vector>
#include <string>
#include "../include/context.hpp"
#include "../include/diagnose.hpp"

namespace minis {
  struct Diagnostic {
    enum EKind { Error, Warning, Note } k;
    Span span; std::string msg;
  };
  static std::vector<Diagnostic> g_diags;

  inline void DIAG(Diagnostic::EKind k, size_t beg, size_t end, std::string m){
    g_diags.emplace_back(k, Span{minis::map_pos(beg), minis::map_pos(end)}, std::move(m));
  }
  inline bool hasErrors(){
    for (auto& d : g_diags) if (d.k==Diagnostic::Error) return true;
    return false;
  }
}