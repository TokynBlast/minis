#include <vector>
#include <unordered_map>
#include <cstring>
#include <cctype>
#include "../include/token.hpp"
#include "../include/context.hpp"
#include "../include/ugly.hpp"
#include "../include/sso.hpp"
#include "../include/process.hpp"
#include "../include/ast.hpp"

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