#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace minis {
  enum class Tok {
    Id, Num, Str,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Colon, Equal,
    Plus, Minus, Star, Slash, Asterisk,
    LT, LE, GT, GE, EQ, NE,
    And, Or,
    Func, Let, If, Elif, ELse, While, Return, Break,
    Cont, Del, Conv, Exit, Try, Except, Finally,
    Lambda, With, Inline, Tail, Void,
    True, False, Null,
    Const, Static,
    Int, Float, Bool, List,
    Import,
    Eof
  };

  struct Token { Tok k; size_t s; size_t e; std::string text; };

  struct TokStream {
    const std::vector<Token>* t=nullptr;
    size_t i=0;
    explicit TokStream(const std::vector<Token>& v){ t=&v; }
    const Token& peek(size_t k=0) const { return (*t)[std::min(i+k, t->size()-1)]; }
    bool match(TKind k){ if (peek().k==k){ ++i; return true; } return false; }
    const Token& expect(TKind k, const char* msg){
      if (peek().k!=k){ DIAG(Diagnostic::Error, peek().s, peek().e, msg); }
      return (*t)[i++];  // advance even on error
    }
  };
}