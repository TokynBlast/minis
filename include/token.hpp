#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

#include "sso.hpp"
#include "ast.hpp"
#include "err.hpp"

namespace lang {

  // Token kinds (keep as-is so callers can do t.k == Tok::LParen etc).
  enum class Tok {
    // Types
    Id, Num, Str, Int, Float, Bool, List, Null, Auto,

    // bool
    True, False,

    // Characters
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, // ( ) { } [ ]
    Comma, Semicolon, Colon,                            // , ; :
    Plus, Minus, Star, FSlash, BSlash, Bang, At, Dollar,// + - * / \ ! @ $
    Hash, Percent, Amp, Karet, UScore, Equal,           // # % & ^ _ =
    Dot, SQuote, DQuote, Pipe,                          // . ' " |
    Tilda,                                              // ~
    LT, LE, GT, GE, EQ, NE, OR, AND,                    // < <= > >= == != || &&
    PP,                                                 // ++

    // Key words
    Func, Let, If, Elif, Else, While, Return, Break,
    With, WAnd, // ... with {} and {} and {} ...
    Cont, Del, Conv,
    Exit, Try, Except, Finally,
    Lambda,
    Import,
    Yield,

    // Declaration types
    Inline, Tail, Void, Const, Static, Dead,

    // Other
    Eof,
    WS,
    Sym
  };

  struct Stmt;

  struct Token {
    Tok k;
    CString text;
    std::uint32_t line = 1; // 1-based
    std::uint32_t col  = 1; // 1-based
    std::shared_ptr<Stmt> meta;

    Token(Tok kind, const char* txt = "")
      : k(kind), text(txt), line(1), col(1), meta(nullptr) {}

    // Compute start line/col from raw byte offset start_off (do NOT store end).
    void set_pos_from_offsets(size_t start_off, size_t /*end_off*/, const CString& src) {
      std::uint32_t ln = 1, cl = 1;
      size_t n = std::min(start_off, src.size());
      const char* src_ptr = src.c_str();
      for (size_t i = 0; i < n; ++i) {
        if (src_ptr[i] == '\n') { ++ln; cl = 1; }
        else ++cl;
      }
      line = ln;
      col  = cl;
    }

    void attach_meta(const std::shared_ptr<Stmt>& m) { meta = m; }
  };

  // Helper: keyword -> canonical "size" (attachable metadata)
  inline size_t KWSize(const char* kw) {
    static const std::unordered_map<const char*, size_t> ks = {
      {"if", 2}, {"elif", 4}, {"else", 4}, {"while", 5},
      {"func", 4}, {"let", 3}, {"return", 6}, {"break", 5},
      {"cont", 8}, {"yield", 5}, {"conv", 4}, {"with", 4}, {"and", 3},
      {"import", 6}, {"try", 3}, {"except", 6}, {"finally", 7},
      {"lambda", 6}, {"inline", 6}, {"tail", 4}, {"void", 4},
      {"const", 5}, {"static", 6}, {"exit", 4}, {"del", 3},
      {"true", 4}, {"false", 5}, {"null", 4}, {"auto", 4}
    };
    auto it = ks.find(kw);
    return it == ks.end() ? 0 : it->second;
  }

  inline void AttachMeta(Token& t) {
    if (t.k != Tok::Id) return;
    size_t ks = KWSize(t.text.c_str());
    if (ks == 0) return;
    auto m = std::make_shared<Stmt>();
    t.attach_meta(m);
  }

  // Simple token stream helper
  struct TokStream {
    const std::vector<Token>* t = nullptr;
    size_t i = 0;
    const char* filename = nullptr; // Store filename for error reporting

    explicit TokStream(const std::vector<Token>& v, const char* fname = nullptr)
      : t(&v), i(0), filename(fname) {}

    const Token& peek(size_t k = 0) const {
      size_t idx = std::min(i + k, t->size() - 1);
      return (*t)[idx];
    }

    bool match(Tok k) {
      if (peek().k == k) { ++i; return true; }
      return false;
    }

    const Token& expect(Tok k, const char* msg) {
      if (peek().k != k) {
        Loc loc;
        loc.line = static_cast<int>(peek().line);
        loc.col  = static_cast<int>(peek().col);
        loc.src = filename ? filename : "<unknown>"; // Use actual filename
        ERR(loc, msg);
      }
      return (*t)[i++]; // advance even on error
    }
  };
}