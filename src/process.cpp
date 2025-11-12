#include <vector>
#include <unordered_map>
#include <cstring>
#include <cctype>
#include "../include/token.hpp"
#include "../include/context.hpp"
#include "../include/ugly.hpp"
#include "../include/sso.hpp"

using namespace lang;

static std::vector<Token> lex(const char* src, size_t src_len){
  std::vector<Token> ts;
  ts.reserve(src_len/3);
  size_t i=0;

  auto push = [&](Tok k, size_t s) {
    ts.emplace_back(k, CString(src + s, i - s));
    ts.back().set_pos_from_offsets(s, i, CString(src, src_len));
  };

  while(i<src_len){
    size_t s=i;
    if(std::isspace((unsigned char)src[i])){
      while(i<src_len && std::isspace((unsigned char)src[i])) ++i;
      ts.emplace_back(Tok::WS, CString(src + s, i - s));
      ts.back().set_pos_from_offsets(s, i, CString(src, src_len));

      const CString& w = ts.back().text;
      int nl = 0;
      for (size_t j = 0; j < w.size(); ++j) {
        if (w[j] == '\n') ++nl;
      }

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

    if(lang::IsIdStart(src[i])){
      ++i;
      while(i<src_len && lang::IsIdCont(src[i])) ++i;
      push(Tok::Id,s);
      continue;
    }

    ++i;
    push(Tok::Sym,s);
  }
  ts.emplace_back(Tok::Eof,"");
  ts.back().set_pos_from_offsets(src_len, src_len, CString(src, src_len));
  return ts;
}

static std::vector<Token> lex(const CString& src){
  return lex(src.c_str(), src.size());
}

// Preprocessing and minification
struct PreprocResult {
  CString out;
  std::vector<size_t> posmap;
};

static PreprocResult uglify_tokens(const char* raw, size_t raw_len) {
  auto toks = lex(raw, raw_len);

  char* minified_result = lang::Ugly(toks);
  if (!minified_result) {
    return { CString(raw, raw_len), std::vector<size_t>() };
  }

  CString minified(minified_result);
  std::free(minified_result);

  // Create position mapping
  std::vector<size_t> posmap(minified.size(), 0);

  return { std::move(minified), std::move(posmap) };
}

static PreprocResult uglify_fallback(const char* raw, size_t raw_len) {
  auto toks = lex(raw, raw_len);

  // Fallback empty mapping
  std::unordered_map<CString, CString> id2mini;

  // Rebuild with spacing logic
  auto need_space = [](const Token& a, const Token& b)->bool{
    auto idlike = [](Tok k){ return k==Tok::Id || k==Tok::Num; };
    return idlike(a.k) && idlike(b.k);
  };

  std::vector<char> buffer;
  buffer.reserve(raw_len/2);
  std::vector<size_t> posmap;
  posmap.reserve(raw_len/2);

  Token prev{Tok::Sym,""};
  for (const Token& t : toks) {
    if (t.k == Tok::Eof) break;
    if (t.k == Tok::WS) continue;

    const char* chunk_data;
    size_t chunk_len;
    CString mapped_id;

    switch (t.k) {
      case Tok::Id: {
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

    // Add space if needed
    if (!buffer.empty() && need_space(prev, t)) {
      buffer.push_back(' ');
      posmap.push_back(posmap.empty() ? 0 : posmap.back());
    }

    // Copy chunk
    for (size_t k = 0; k < chunk_len; ++k) {
      buffer.push_back(chunk_data[k]);
      posmap.push_back(posmap.empty() ? k : posmap.back() + (k == 0 ? 1 : 0));
    }

    prev = t;
  }

  // Convert buffer to CString
  buffer.push_back('\0');
  CString out(buffer.data());

  return { std::move(out), std::move(posmap) };
}

static PreprocResult uglify(const CString& raw) {
  return uglify_tokens(raw.c_str(), raw.size());
}

// Public API
namespace lang {
  CString process(const CString& source) {
    PreprocResult result = uglify(source);
    return std::move(result.out);
  }
}