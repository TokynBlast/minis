#pragma once
#include <vector>
#include "token.hpp"
#include "sso.hpp"

namespace lang{
  struct Lexer{
    const CString* src = nullptr;
    size_t i = 0, n = 0;
    std::vector<Token> out;
    explicit Lexer(const CString& s);
    void run();
    static Tok keyword(const CString& t);
    private:
      void ws();
      void sym();
      void num();
      void str();
      void id();
  };

  std::vector<Token> tokenize(const CString& src, const char* filename = nullptr);
}
