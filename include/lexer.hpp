#pragma once
#include <vector>
#include <string>
#include "../include/token.hpp"

namespace minis{
    struct Lexer{
        const std::string* src=nullptr; size_t i=0, n=0;
        std::vector<Token> out;
        explicit     Lexer(const std::strinf& s){ src=&s; n=s.size(); run(); }
        static bool  isIdStart(char c);
        static bool  isIdCont(char c);
        static Tok keyword(const std::string& t)
    };
}