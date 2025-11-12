#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "../include/lexer.hpp"
#include "../include/token.hpp"
#include "../include/err.hpp"
#include "../include/ast.hpp"

namespace lang {

  static bool IsIdStart(char c) {
    return isalpha(static_cast<unsigned char>(c)) || c == '_';
  }
  static bool IsIdCont(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
  }

  static const std::unordered_map<std::string, Tok> keywords = {
    {"func", Tok::Func},
    {"let", Tok::Let},
    {"if", Tok::If},
    {"elif", Tok::Elif},
    {"else", Tok::Else},
    {"while", Tok::While},
    {"return", Tok::Return},
    {"break", Tok::Break},
    {"continue", Tok::Cont},
    {"del", Tok::Del},
    {"conv", Tok::Conv},
    {"exit", Tok::Exit},
    {"try", Tok::Try},
    {"except", Tok::Except},
    {"finally", Tok::Finally},
    {"lambda", Tok::Lambda},
    {"with", Tok::With},
    {"and", Tok::WAnd},
    {"inline", Tok::Inline},
    {"tail", Tok::Tail},
    {"void", Tok::Void},
    {"true", Tok::True},
    {"false", Tok::False},
    {"null", Tok::Null},
    {"const", Tok::Const},
    {"static", Tok::Static},
    {"int", Tok::Int},
    {"float", Tok::Float},
    {"bool", Tok::Bool},
    {"str", Tok::Str},
    {"list", Tok::List},
    {"auto", Tok::Auto},
    {"import", Tok::Import},
    {"yield", Tok::Yield}
  };

  static Tok keywordTok(const std::string& t) {
    auto it = keywords.find(t);
    return it != keywords.end() ? it->second : Tok::Id;
  }

  static void set_pos_from_offsets(Token& tok, size_t start_off, const char* src) {
    tok.set_pos_from_offsets(start_off, start_off, src);
  }

  std::vector<Token> tokenize(const char* src, size_t src_len, const char* /*filename*/) {
    std::vector<Token> out;
    out.reserve(src_len / 3 + 8);
    size_t i = 0;
    const size_t n = src_len;

    while (i < n) {
      const size_t s = i;
      const unsigned char c = static_cast<unsigned char>(src[i]);

      // whitespace
      if (isspace(c)) {
        while (i < n && isspace(static_cast<unsigned char>(src[i]))) ++i;
        out.emplace_back(Tok::WS, std::string(src + s, i - s));
        set_pos_from_offsets(out.back(), s, src);
        auto m = std::make_shared<Stmt>();
        m->s = i - s;
        out.back().attach_meta(m);
        continue;
      }

      // line comment //
      if (i + 1 < n && src[i] == '/' && src[i+1] == '/') {
        const size_t start = i;
        i += 2;
        while (i < n && src[i] != '\n') ++i;
        out.emplace_back(Tok::WS, std::string(src + start, i - start));
        set_pos_from_offsets(out.back(), start, src);
        auto m = std::make_shared<Stmt>();
        m->s = i - start;
        out.back().attach_meta(m);
        continue;
      }

      // block comment /* ... */
      if (i + 1 < n && src[i] == '/' && src[i+1] == '*') {
        const size_t start = i;
        i += 2;
        int depth = 1;
        while (i + 1 < n && depth > 0) {
          if (src[i] == '/' && src[i+1] == '*') { depth++; i += 2; }
          else if (src[i] == '*' && src[i+1] == '/') { depth--; i += 2; }
          else ++i;
        }
        out.emplace_back(Tok::WS, std::string(src + start, i - start));
        set_pos_from_offsets(out.back(), start, src);
        auto m = std::make_shared<Stmt>();
        m->s = i - start;
        out.back().attach_meta(m);
        continue;
      }

      // strings
      if (src[i] == '"' || src[i] == '\'') {
        const size_t start = i;
        const char q = src[i++];
        bool esc = false;
        while (i < n) {
          const char ch = src[i++];
          if (esc) { esc = false; continue; }
          if (ch == '\\') { esc = true; continue; }
          if (ch == q) break;
        }
        out.emplace_back(Tok::Str, std::string(src + start, i - start));
        set_pos_from_offsets(out.back(), start, src);
        continue;
      }

      // numbers
      if (isdigit(static_cast<unsigned char>(src[i])) ||
         ((src[i] == '+' || src[i] == '-') && i + 1 < n && isdigit(static_cast<unsigned char>(src[i+1])))) {
        const size_t start = i;
        ++i;
        while (i < n && (isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) ++i;
        out.emplace_back(Tok::Num, std::string(src + start, i - start));
        set_pos_from_offsets(out.back(), start, src);
        continue;
      }

      // identifiers / keywords
      if (IsIdStart(src[i])) {
        const size_t start = i;
        ++i;
        while (i < n && IsIdCont(src[i])) ++i;
        std::string text(src + start, i - start);
        const Tok k = keywordTok(text);
        out.emplace_back(k, std::move(text));
        set_pos_from_offsets(out.back(), start, src);
        if (k != Tok::Id) {
          auto m = std::make_shared<Stmt>();
          m->s = i - start;
          out.back().attach_meta(m);
        }
        continue;
      }

      // two-char ops
      if (i + 1 < n) {
        const char ch1 = src[i], ch2 = src[i+1];
        if (ch1 == '&' && ch2 == '&') {
          out.emplace_back(Tok::AND, "&&");
          set_pos_from_offsets(out.back(), i, src);
          i += 2; continue;
        }
        if (ch1 == '|' && ch2 == '|') {
          out.emplace_back(Tok::OR, "||");
          set_pos_from_offsets(out.back(), i, src);
          i += 2; continue;
        }
        if (ch1 == '=' && ch2 == '=') {
          out.emplace_back(Tok::EQ, "==");
          set_pos_from_offsets(out.back(), i, src);
          i += 2; continue;
        }
        if (ch1 == '!' && ch2 == '=') {
          out.emplace_back(Tok::NE, "!=");
          set_pos_from_offsets(out.back(), i, src);
          i += 2; continue;
        }
        if (ch1 == '<' && ch2 == '=') {
          out.emplace_back(Tok::LE, "<=");
          set_pos_from_offsets(out.back(), i, src);
          i += 2; continue;
        }
      }

      // single-char symbols
      const size_t start = i;
      const char ch = src[i++];
      Tok tk = Tok::Sym;
      switch (ch) {
        case '(': tk = Tok::LParen; break;
        case ')': tk = Tok::RParen; break;
        case '{': tk = Tok::LBrace; break;
        case '}': tk = Tok::RBrace; break;
        case '[': tk = Tok::LBracket; break;
        case ']': tk = Tok::RBracket; break;
        case ',': tk = Tok::Comma; break;
        case ';': tk = Tok::Semicolon; break;
        case ':': tk = Tok::Colon; break;
        case '+': tk = Tok::Plus; break;
        case '-': tk = Tok::Minus; break;
        case '*': tk = Tok::Star; break;
        case '/': tk = Tok::FSlash; break;
        case '\\':tk = Tok::BSlash; break;
        case '!': tk = Tok::Bang; break;
        case '<': tk = Tok::LT; break;
        case '>': tk = Tok::GT; break;
        case '$': tk = Tok::Dollar; break;
        case '_': tk = Tok::UScore; break;
        case '&': tk = Tok::Amp; break;
        case '^': tk = Tok::Karet; break;
        case '%': tk = Tok::Percent; break;
        case '.': tk = Tok::Dot; break;
        case '\'': tk = Tok::SQuote; break;
        case '"': tk = Tok::DQuote; break;
        default: {
          Token tmp(Tok::WS, std::string(1, ch));
          set_pos_from_offsets(tmp, start, src);
          Loc loc;
          strncpy(loc.src, "", sizeof(loc.src)-1);
          loc.line = tmp.line;
          loc.col  = tmp.col;
          char msg[64];
          snprintf(msg, sizeof(msg), "unknown char '%c'", ch);
          WARN(loc, std::string(msg));
          continue;
        }
      }
      out.emplace_back(tk, std::string(1, ch));
      set_pos_from_offsets(out.back(), start, src);
    }

    out.emplace_back(Tok::Eof, "");
    set_pos_from_offsets(out.back(), n, src);
    return out;
  }

  // Overload for std::string compatibility
  std::vector<Token> tokenize(const std::string& src, const char* filename) {
    return tokenize(src.c_str(), src.size(), filename);
  }

  lang::Lexer::Lexer(const std::string& s) {
    src = &s;
    i = 0;
    n = s.size();
    out.clear();
  }

  void lang::Lexer::run() {
    out = tokenize(*src, nullptr);
  }

  Tok lang::Lexer::keyword(const std::string& t) {
    return keywordTok(t);
  }

} // namespace lang
