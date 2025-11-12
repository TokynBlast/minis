#pragma once
#include <cstdint>
#include <algorithm>

#include "sso.hpp"
#include "err.hpp"
#include "token.hpp"
#include "ast.hpp"

namespace lang {

inline Loc BuildLoc(const Token& t, const char* filename) {
  Loc loc;
  loc.line = static_cast<uint32_t>(t.line);
  loc.col  = static_cast<uint32_t>(t.col);
  if (filename && filename[0]) {
    std::strncpy(loc.src, filename, sizeof(loc.src) - 1);
    loc.src[sizeof(loc.src) - 1] = '\0';
  } else {
    loc.src[0] = '\0';
  }
  return loc;
}

inline Loc BuildLocEnd(const Token& t, const Stmt* meta, const char* filename) {
  Loc loc = BuildLoc(t, filename);

  if (!meta) return loc;

  const char* txt = t.text.c_str();
  size_t txt_len = t.text.size();
  size_t nl_count = 0;
  for (size_t i = 0; i < txt_len; ++i) if (txt[i] == '\n') ++nl_count;

  if (nl_count > 0) {
    loc.line = static_cast<uint32_t>(t.line + nl_count);
    const char* p = std::strrchr(txt, '\n');
    if (p) {
      size_t after = std::strlen(p + 1);
      loc.col = static_cast<uint32_t>(1 + after);
      return loc;
    }
  }

  std::size_t extent = meta->s;
  if (extent == 0) return loc;
  loc.col = static_cast<uint32_t>(t.col + (extent > 0 ? extent - 1 : 0));
  return loc;
}

inline void err_tok(const Token& t, const char* filename, const char* msg) {
  Loc loc = BuildLoc(t, filename);
  ERR(loc, msg);
}

inline void err_end(const Token& t, const Stmt* meta, const char* filename, const char* msg) {
  Loc loc = BuildLocEnd(t, meta, filename);
  ERR(loc, msg);
}

}