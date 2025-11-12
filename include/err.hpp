#pragma once
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include "sso.hpp"

namespace lang {

struct Loc {
  int line = 1, col = 1;
  CString src; // Using CString instead of char array
};

struct Source {
  CString name, text;
  std::vector<size_t> line_starts;

  Source() = default;
  Source(CString n, CString t): name(std::move(n)), text(std::move(t)) {
    line_starts.push_back(0);
    for (size_t i = 0; i < text.size(); ++i)
      if (text[i] == '\n') line_starts.push_back(i + 1);
  }

  Loc loc(size_t index) const {
    Loc result;
    result.src = name.c_str();
    if (line_starts.empty()) return result;
    if (index > text.size()) index = text.size();
    auto it = std::upper_bound(line_starts.begin(), line_starts.end(), index);
    if (it == line_starts.begin()) {
      result.col = (int)index + 1;
      return result;
    }
    size_t ln = (it - line_starts.begin()) - 1;
    size_t col0 = index - line_starts[ln];
    result.line = (int)ln + 1;
    result.col = (int)col0 + 1;
    return result;
  }

  CString line(int ln) const {
    if (ln < 1 || (size_t)ln > line_starts.size()) return CString("");
    size_t s = line_starts[ln - 1];
    size_t e = (size_t)ln < line_starts.size() ? line_starts[ln] : text.size();
    if (e > s && text[e - 1] == '\n') --e;
    return CString(text.c_str() + s, e - s);
  }
};

static bool has_error = false;

void err(const Loc& loc, const CStringr& msg, int type = 1) {
  const char* colors[] = {"", "\x1b[1;31merror\x1b[0m", "\x1b[1;36mwarning\x1b[0m", "\x1b[1;32mnote\x1b[0m"};
  if (type == 1) has_error = true;
  std::cout << loc.src << ":" << loc.line << ":" << loc.col << ": " << colors[type] << ": " << msg << "\n";
}

#define ERR(LOC, MSG) err(LOC, MSG, 1)
#define WARN(LOC, MSG) err(LOC, MSG, 2)
#define NOTE(LOC, MSG) err(LOC, MSG, 3)

}
