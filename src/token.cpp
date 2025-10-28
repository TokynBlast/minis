enum class TKind {
  Id, Num, Str,
  LParen, RParen, LBrace, RBrace, LBracket, RBracket,
  Comma, Semicolon, Colon, Equal,
  Plus, Minus, Star, Slash, Bang,
  LT, LE, GT, GE, EQEQ, NE,
  AndAnd, OrOr,
  Kw_func, Kw_let, Kw_if, Kw_elif, Kw_else, Kw_while, Kw_return, Kw_break,
  Kw_continue, Kw_del, Kw_conv, Kw_exit, Kw_try, Kw_except, Kw_finally,
  Kw_lambda, Kw_with, Kw_and, Kw_inline, Kw_tailcall, Kw_void,
  Kw_true, Kw_false, Kw_null,
  Kw_const, Kw_static,
  Kw_int, Kw_float, Kw_bool, Kw_str, Kw_list, Kw_null,
  Eof
};

struct Token { TKind k; size_t s; size_t e; std::string text; };

struct TokStream {
  const std::vector<Token>* t=nullptr; size_t i=0;
  explicit TokStream(const std::vector<Token>& v){ t=&v; }
  const Token& peek(size_t k=0) const { return (*t)[std::min(i+k, t->size()-1)]; }
  bool match(TKind k){ if (peek().k==k){ ++i; return true; } return false; }
  const Token& expect(TKind k, const char* msg){
    if (peek().k!=k){ DIAG(Diagnostic::Error, peek().s, peek().e, msg); }
    return (*t)[i++]; // advance even on error to avoid infinite loops
  }
};
