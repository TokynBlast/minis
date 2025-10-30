// ====== tiny diagnostics + suggestions ======
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <exception>

struct Span { size_t beg=0, end=0; };                // [beg, end)
struct Loc  { int line=1, col=1; };

struct Source {
  std::string name, text; std::vector<size_t> line_starts;
  Source(std::string n, std::string t): name(std::move(n)), text(std::move(t)){
    line_starts.push_back(0);
    for (size_t i=0;i<text.size();++i) if (text[i]=='\n') line_starts.push_back(i+1);
  }
  size_t line_count() const { return line_starts.size(); }
  Loc loc_at(size_t index) const {
    if (line_starts.empty()) return {1, 1};
    if (index > text.size()) index=text.size();
    auto it = std::upper_bound(line_starts.begin(), line_starts.end(), index);
    if (it == line_starts.begin()) return {1, (int)index + 1};
    size_t ln = (it-line_starts.begin())-1;
    if (ln >= line_starts.size()) ln = line_starts.size() - 1;
    size_t col0 = index - line_starts[ln];
    return {(int)ln+1, (int)col0+1};
  }
  std::string line_str(int ln) const {
    if (ln<1 || (size_t)ln>line_starts.size()) return {};
    size_t s=line_starts[ln-1], e=(size_t)ln<line_starts.size()?line_starts[ln]:text.size();
    if (e> s && text[e-1]=='\n') --e;
    return text.substr(s,e-s);
  }
};

struct ScriptError : std::exception {
  std::string message; Span span; std::vector<std::string> notes, suggestions;
  ScriptError(std::string m, Span sp): message(std::move(m)), span(sp) {}
  const char* what() const noexcept override { return message.c_str(); }
};

// --- minimal renderer (prints file:line:col + underline) ---
inline std::string render_diagnostic(const Source& src, const ScriptError& err, int ctx=1){
  Span sp = err.span; if (sp.end < sp.beg) sp.end = sp.beg;
  if (sp.end > src.text.size()) sp.end = src.text.size();
  auto lc = src.loc_at(sp.beg);
  std::ostringstream out;
  out << src.name << ":" << lc.line << ":" << lc.col << ": \x1b[1;31merror: " << err.message << "\x1b[0m\n";
  int l0 = std::max(1, lc.line-ctx), l1 = std::min((int)src.line_count(), lc.line+ctx);
  for(int ln=l0; ln<=l1; ++ln){
    auto s = src.line_str(ln);
    out << " " << ln << " | " << s << "\n";
    if (ln==lc.line){
      size_t ls = src.line_starts[ln-1];
      int cb = (int)(sp.beg - ls) + 1, ce = (int)(sp.end - ls) + 1;
      if (cb<1) cb=1;
      if (ce<cb) ce=cb;
      out << " " << std::string(std::to_string(ln).size(),' ')
          << " | " << std::string(cb-1,' ') << "^" << std::string(std::max(0, ce-cb),'~') << "\n";
    }
  }
  for (auto& n: err.notes) out << "note: " << n << "\n";
  for (auto& s: err.suggestions) out << "help: did you mean '" << s << "'?\n";
  return out.str();
}

// --- suggestions (Levenshtein) ---
inline int levenshtein(const std::string& a, const std::string& b){
  size_t n=a.size(), m=b.size(); std::vector<int> dp(m+1);
  for(size_t j=0;j<=m;++j) dp[j]=(int)j;
  for(size_t i=1;i<=n;++i){
    int prev=dp[0]; dp[0]=(int)i;
    for(size_t j=1;j<=m;++j){
      int cur=dp[j]; int cost=(a[i-1]==b[j-1])?0:1;
      dp[j]=std::min({dp[j]+1, dp[j-1]+1, prev+cost}); prev=cur;
    }
  }
  return dp[m];
}
inline std::vector<std::string> best_suggestions(
  const std::string& key, const std::vector<std::string>& dict, int k=3){
  std::vector<std::pair<int,std::string>> v; v.reserve(dict.size());
  for(auto& w: dict) v.emplace_back(levenshtein(key,w), w);
  std::sort(v.begin(), v.end(), [](auto& L, auto& R){ return L.first<R.first; });
  std::vector<std::string> out; for(int i=0;i<(int)v.size() && i<k; ++i) out.push_back(v[i].second);
  return out;
}

// --- unified throw helper + ONE macro ---
// Normalize "position or span" to a real Span
inline Span minis_make_span(Span s){ return s; }
inline Span minis_make_span(size_t i){ return Span{i, i+1}; } // e.g. P.i

[[noreturn]] inline void minis_throw(const Source& src,
                        Span sp,
                        const std::string& id_and_msg,
                        std::initializer_list<std::string> notes = {},
                        std::initializer_list<std::string> suggs = {})
{
  ScriptError e(id_and_msg, sp);
  e.notes.assign(notes.begin(), notes.end());
  e.suggestions.assign(suggs.begin(), suggs.end());
  e.message = render_diagnostic(src, e, 1);   // bake pretty text into message
  throw e;
}

#define MINIS_ERR(IDSTR, SRC, POS_OR_SPAN, MSG, ...) \
  minis_throw((SRC), minis_make_span(POS_OR_SPAN), std::string(IDSTR) + " " + (MSG), ##__VA_ARGS__)
