#include <vector>
#include <unordered_map>
#include <cstring>
#include "../include/token.hpp"
#include "../include/context.hpp"
#include "../include/ugly.hpp"

using namespace lang;

static std::vector<Token> lex(const char* src, size_t src_len){
  std::vector<Token> ts;
  ts.reserve(src_len/3);
  size_t i=0;

  auto push = [&](Tok k, size_t s) {
    ts.emplace_back(k, std::string(src + s, i - s));
    ts.back().set_pos_from_offsets(s, i, std::string(src, src_len));
  };

  while(i<src_len){
    size_t s=i;
    if(std::isspace((unsigned char)src[i])){
      while(i<src_len && std::isspace((unsigned char)src[i])) ++i;
      ts.emplace_back(Tok::WS, std::string(src + s, i - s));
      ts.back().set_pos_from_offsets(s, i, std::string(src, src_len));

      const std::string& w = ts.back().text;
      int nl = 0;
      for (char c : w) if (c == '\n') ++nl;

      if (nl > 0) {
        auto ws = std::make_shared<WS>();
        ws->s = w.size();
        ws->NL = nl;
        ts.back().attach_meta(ws);
      }
      continue;
    }

    if(i+1<src_len && src[i]=='/' && src[i+1]=='/'){
      i+=2;
      while(i<src_len && src[i]!='\n') ++i;
      continue;
    }

    if(i+1<src_len && src[i]=='/' && src[i+1]=='*'){
      i+=2; int depth=1;
      while(i+1<src_len && depth>0){
        if(src[i]=='/' && src[i+1]=='*'){ depth++; i+=2; }
        else if(src[i]=='*' && src[i+1]=='/'){ depth--; i+=2; }
        else ++i;
      }
      continue;
    }

    if(src[i]=='"' || src[i]=='\''){
      char q=src[i++];
      bool esc=false;
      while(i<src_len){
        char c=src[i++];
        if(esc){ esc=false; continue; }
        if(c=='\\'){ esc=true; continue; }
        if(c==q) break;
      }
      push(Tok::Str,s);
      continue;
    }

    if(std::isdigit((unsigned char)src[i]) || ((src[i]=='+'||src[i]=='-') && i+1<src_len && std::isdigit((unsigned char)src[i+1]))){
      ++i;
      while(i<src_len && (std::isdigit((unsigned char)src[i]) || src[i]=='.')) ++i;
      push(Tok::Num,s);
      continue;
    }

    if(IsIdStart(src[i])){
      ++i;
      while(i<src_len && IsIdCont(src[i])) ++i;
      push(Tok::Id,s);
      continue;
    }

    ++i;
    push(Tok::Sym,s);
  }
  ts.emplace_back(Tok::Eof,"");
  ts.back().set_pos_from_offsets(src_len, src_len, std::string(src, src_len));
  return ts;
}

static std::vector<Token> lex(const std::string& src){
  return lex(src.c_str(), src.size());
}

// Preprocessing and minification
struct PreprocResult {
  std::string out;
  std::vector<size_t> posmap;   // out[i] -> raw offset
};

static PreprocResult uglify(const char* raw, size_t raw_len) {
  auto toks = lex(raw, raw_len);

  // auto plan = uglify(toks);

  // fallback empty mapping if plan doesn't provide one yet
  std::unordered_map<std::string, std::string> id2mini;
  // If `plan` actually has a mapping (e.g. plan.id2mini), assign it here:
  // id2mini = plan.id2mini; // uncomment/adapt when plan's type is known

  // Rebuild with spacing logic, but track positions.
  auto need_space = [](const Token& a, const Token& b)->bool{
    auto idlike = [](Tok k){ return k==Tok::Id || k==Tok::Num; };
    return idlike(a.k) && idlike(b.k);
  };

  std::string out;
  std::vector<size_t> posmap;
  out.reserve(raw_len/2);
  posmap.reserve(raw_len/2);

  Token prev{Tok::Sym,""};
  size_t toks_count = toks.size();
  for (size_t i=0; i<toks_count; ++i) {
    const Token& t = toks[i];
    if (t.k == Tok::Eof) break;
    if (t.k == Tok::WS) continue;

    const char* chunk_data;
    size_t chunk_len;
    std::string mapped_id;

    switch (t.k) {
      case Tok::Id: {
        // Use id2mini mapping if available, otherwise preserve original identifier.
        auto it = id2mini.find(t.text);
        if (it != id2mini.end()) {
          mapped_id = it->second;
          chunk_data = mapped_id.c_str();
          chunk_len = mapped_id.size();
        } else {
          chunk_data = t.text.c_str();
          chunk_len = t.text.size();
        }
      } break;
      case Tok::Str:
      case Tok::Num:
      case Tok::Sym:
        chunk_data = t.text.c_str();
        chunk_len = t.text.size();
        break;
      default:
        chunk_data = "";
        chunk_len = 0;
        break;
    }

    // space if needed
    if (!out.empty() && need_space(prev, t)) {
      out.push_back(' ');
      // best effort: map space to the start of next token
      posmap.push_back(posmap.empty() ? 0 : posmap.back());
    }

    // copy chunk and attach positions
    for (size_t k = 0; k < chunk_len; ++k) {
      out.push_back(chunk_data[k]);
      // Since we don't have direct offset access anymore, use incremental mapping
      posmap.push_back(posmap.empty() ? k : posmap.back() + (k == 0 ? 1 : 0));
    }

    prev = t;
  }
  return { std::move(out), std::move(posmap) };
}

static PreprocResult uglify(const std::string& raw) {
  return uglify(raw.c_str(), raw.size());
}
