#include <vector>
#include <string>

struct Diagnostic {
  enum Kind { Error, Warning, Note } k;
  Span span; std::string msg;
};
static std::vector<Diagnostic> g_diags;

inline void DIAG(Diagnostic::Kind k, size_t beg, size_t end, std::string m){
  g_diags.push_back(k, Span{map_pos(beg), map_pos(end)}, std::move(m));
}
inline bool hasErrors(){
  for (auto& d : g_diags) if (d.k==Diagnostic::Error) return true;
  return false;
}
