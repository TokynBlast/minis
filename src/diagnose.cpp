#include <vector>
#include <string>
#include "../include/context.hpp"
#include "../include/diagnose.hpp"

namespace minis {
  std::vector<Diagnostic> g_diags;

  inline void DIAG(Diagnostic::EKind k, size_t beg, size_t end, std::string msg){
    g_diags.emplace_back(k, Span{minis::map_pos(beg), minis::map_pos(end)}, std::move(msg));
  }
}