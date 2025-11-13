#pragma once
#include "sso.hpp"

namespace lang {
  struct Loc {
    int line = 1, col = 1;
    CString src;
  };

  struct Source {
    CString name, text;
    std::vector<size_t> line_starts;

    Source() = default;
    Source(CString n, CString t);
    Loc loc(size_t index) const;
    CString line(int ln) const;
  };

  extern bool has_error;

  void err(const Loc& loc, const CString& msg, int type = 1);

  #define ERR(LOC, MSG) err(LOC, MSG, 1)
  #define WARN(LOC, MSG) err(LOC, MSG, 2)
  #define NOTE(LOC, MSG) err(LOC, MSG, 3)
}