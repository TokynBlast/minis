#pragma once

#include <vector>
#include <cstring>

#include "token.hpp"
#include "err.hpp"

namespace lang {

  class Cursor {
  private:
    const std::vector<Token>& toks;
    size_t i = 0;
    const Token* t = nullptr;

    static const Token& eof_token() {
      static Token v = [](){
        Token tt{Tok::Eof, ""};
        tt.line = 0;
        tt.col  = 0;
        return tt;
      }();
      return v;
    }

  public:
    explicit Cursor(const std::vector<Token>& T) : toks(T) {
      if (!toks.empty()) t = &toks[0];
      else t = &eof_token();
    }

    bool atEnd() const { return i >= toks.size(); }
    const Token& curr() const { return *t; }

    void adv(size_t forward = 1) {
      if (i + forward < toks.size()) {
        i += forward;
        t = &toks[i];
      } else {
        i = toks.size();
        t = &eof_token();
      }
    }

    // return by value to avoid returning a reference to a temporary
    Tok peek(size_t forward = 1) const {
      if (i + forward >= toks.size()) return Tok::Eof;
      return toks[i + forward].k;
    }

    bool check(Tok k) const {
      return !atEnd() && t->k == k;
    }

    bool match(Tok k) {
      if (check(k)) { adv(); return true; }
      return false;
    }

    // expect returns true on success, false on error (and emits error)
    bool expect(Tok k, const Source& src, const char* msg) {
      if (!check(k)) {
        Loc loc;
        loc.src = src.name.c_str();
        loc.line = static_cast<int>(t->line);
        loc.col  = static_cast<int>(t->col);
        ERR(loc, msg);
        return false;
      } else {
        adv();
        return true;
      }
    }
  };

}