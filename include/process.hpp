#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include "token.hpp"
#include "context.hpp"
#include "ast.hpp"
#include "sso.hpp"

namespace lang {

// Lexer function
std::vector<Token> lex(const CString& src);

// Preprocessing and minification result
struct PreprocResult {
    CString out;
    std::vector<size_t> posmap;   // out[i] -> raw offset
};

// Uglify/minify function
PreprocResult uglify(const CString& raw);

// Helper functions (may be defined in ugly.cpp)
bool IsIdStart(char c);
bool IsIdCont(char c);
bool is_builtin(const CString& text);

// Uglify plan structure (may be defined in ugly.cpp)
struct UglifyPlan {
    std::unordered_map<CString, CString> id2mini;
};

// Overloaded uglify function for tokens
UglifyPlan uglify(const std::vector<Token>& toks);

}
