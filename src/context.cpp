#include "../include/context.hpp"
#include "../include/diagnose.hpp"
namespace minis {
  std::vector<std::size_t> g_posmap;
  inline bool hasErrors(){
    for (auto& d : g_diags) if (d.k==Diagnostic::Error) return true;
    return false;
  }
}
