#include "../include/context.hpp"
namespace minis {
  static Context* G = nullptr;
  Context& ctx(){ static Context C; if(!G) G=&C; return *G; }

  void diag(Diagnostic::EKind k, size_t beg, size_t end, std::string msg){
    ctx().diags.push_back({k, Span{map_pos(beg), map_pos(end)}, std::move(msg)});
  }
}
