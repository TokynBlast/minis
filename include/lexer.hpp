#pragma once
#include <vector>
#include "token.hpp"
#include "sso.hpp"

namespace lang {

class Lexer {
private:
    const CString* src = nullptr;
    size_t i = 0, n = 0;
    std::vector<Token> out;

public:
    explicit Lexer(const CString& s);
    void run();
    const std::vector<Token>& tokens() const { return out; }
    static Tok keyword(const CString& t);
};

}
