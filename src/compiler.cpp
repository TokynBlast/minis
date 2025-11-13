#include <unordered_map>

#include "../include/io.hpp"
#include "../include/token.hpp"
#include "../include/err.hpp"
#include "../include/types.hpp"
#include "../include/value.hpp"
#include "../include/bytecode.hpp"
#include "../include/cursor.hpp"
#include "../include/sso.hpp"

using namespace lang;

struct CompilerFnInfo{
  CString name;
  uint64_t entry=0;
  std::vector<CString> params;
  bool isVoid=false;
  bool typed=false;
  Type ret=Type::Int;
  bool isInline=false;
  bool tail=false;
  std::vector<std::pair<Type, std::optional<Value>>> param_types;
};

struct Compiler {
  FILE* out=nullptr;
  std::vector<CompilerFnInfo> fns;
  std::unordered_map<CString,size_t> fnIndex;
  bool inWith;

  std::vector<Token> toks;
  size_t i = 0; // Current token index (replacing Pos)

  Compiler(const std::vector<Token>& tokens): toks(tokens) {}

  const Token& t() const { return toks[i]; } // Helper to get current token

  // Helper to get current location from token
  // Token contains: line, col from AST
  // AST will have WS nodes with space count for accurate positioning
  inline Loc getCurrentLoc() const {
    if (i >= toks.size()) {
      // End of file case - use last token or default
      if (toks.empty()) {
        return Loc{1, 1, "SRCNAME"}; // line, col, filename
      }
      const auto& lastTok = toks.back();
      return Loc{static_cast<int>(lastTok.line), static_cast<int>(lastTok.col), "SRCNAME"};
    }

    const auto& tok = toks[i];
    return Loc{static_cast<int>(tok.line), static_cast<int>(tok.col), "SRCNAME"};
  }

  uint64_t table_offset_pos=0, fn_count_pos=0, entry_main_pos=0;

  inline Type parseType(){
    if(t().k==Tok::Int) return Type::Int;
    if(t().k==Tok::Float) return Type::Float;
    if(t().k==Tok::Bool) return Type::Bool;
    if(t().k==Tok::Str) return Type::Str;
    if(t().k==Tok::List) return Type::List;
    if(t().k==Tok::Null) return Type::Null;
    ERR(getCurrentLoc(), "unknown type (Types are int|float|bool|str|list|null)");
    return Type::Int;
  }

  inline void emit_u8 (uint8_t v){ write_u8(out,v); }
  inline void emit_u64(uint64_t v){ write_u64(out,v); }
  inline void emit_s64(int64_t v){ write_s64(out,v); }
  inline void emit_f64(double v){ write_f64(out,v); }
  inline void emit_str(const CString& s){ write_str(out, s.c_str()); }
  inline uint64_t tell() const { return (uint64_t)ftell(out); }
  inline void seek(uint64_t pos){ fseek(out,(long)pos,SEEK_SET); }

  // --- Expressions -> bytecode ---
  inline void Expr(){ LogicOr(); }
  inline void LogicOr(){ LogicAnd(); while(t().k ==Tok::OR){ ++i; LogicAnd(); this->emit_u64(OR); } }
  inline void LogicAnd(){ Equality(); while(t().k==Tok::AND){ ++i; Equality(); this->emit_u64(AND); } }
  inline void Equality(){
    AddSub();
    while(true){
      if(t().k==Tok::EQ){ ++i; AddSub(); this->emit_u64(EQ); }
      else if(t().k==Tok::NE){ ++i; AddSub(); this->emit_u64(NE); }
      else if(t().k==Tok::GE){ ++i; AddSub(); this->emit_u64(LE); this->emit_u64(MUL); this->emit_u64(PUSH_I); this->emit_s64(-1); this->emit_u64(MUL); }
      else if(t().k==Tok::GT) { ++i; AddSub(); this->emit_u64(LT); this->emit_u64(MUL); this->emit_u64(PUSH_I); this->emit_s64(-1); this->emit_u64(MUL); }
      else if(t().k==Tok::LE){ ++i; AddSub(); this->emit_u64(LE); }
      else if(t().k==Tok::LT) { ++i; AddSub(); this->emit_u64(LT); }
      else break;
    }
  }

  inline void AddSub(){
    MulDiv();
    while(true){
      if(t().k==Tok::Plus){ ++i; MulDiv(); this->emit_u64(ADD); } // +
      else if(t().k==Tok::Minus){ ++i; MulDiv(); this->emit_u64(SUB); } // -
      else break;
    }
  }

  inline void MulDiv(){
    Factor();
    while(true){
      if(t().k==Tok::Star){ ++i; Factor(); this->emit_u64(MUL); } // *
      else if(t().k==Tok::FSlash){ ++i; Factor(); this->emit_u64(DIV); } // /
      else break;
    }
  }

  inline void ListLit() {
    size_t count = 0;

    if (t().k == Tok::RBracket) {
      this->emit_u64(MAKE_LIST);
      this->emit_u64(0);
      ++i;
      return;
    }

    while (i < toks.size()) {
      Expr();
      ++count;

      if (t().k == Tok::RBracket) break;

      // This way, we can determine if it's all commas and warn that a dead list is better to use
      size_t spot=1;
      while(t().k != Tok::RBracket){
        if(i + spot >= toks.size() || toks[i + spot].k != Tok::Comma) break;
        ++spot;
        if (t().k == Tok::Semicolon||t().k==Tok::RBrace||t().k==Tok::Eof) { // list cut off early
          ERR(getCurrentLoc(), "List statement unfinished");
          while (i < toks.size() &&
                 t().k != Tok::RBracket &&
                 t().k != Tok::RBrace &&
                 t().k != Tok::Semicolon &&
                 t().k != Tok::Eof) {
            ++i;
          }
          break;
        }
      }
      // Empty values are assigned null
      // TESTME: Giving a list empty values set to null (Meaning we already know how big the list will be)
      NOTE(getCurrentLoc(), "Empty list values are assigned null");
      // rather than updating size and adding a value
      if (i + 1 < toks.size() && toks[i].k == Tok::Comma && toks[i + 1].k == Tok::Comma) {
        NOTE(getCurrentLoc(), "Empty list values are assigned null");
        ++i;
        continue;
      }

      if (t().k == Tok::Comma) {
        ++i;
        continue;
      }

      ERR(getCurrentLoc(), "Expected ',' or ']'");
      ++i;
    }

    this->emit_u64(MAKE_LIST);
    this->emit_u64(count);
  }

  inline void Factor() {
    if (i >= toks.size()) return;

    switch (t().k) {
      // --- Grouped expressions: ( expr )
      case Tok::LParen: {
        ++i;
        Expr();

        // expect ')'
        if (i >= toks.size() || toks[i].k != Tok::RParen) {
          ERR(getCurrentLoc(), "Missing closing ')'");
          // Recover to next likely token
          while (i < toks.size()) {
            Tok k = toks[i].k;
            if (k == Tok::RParen || k == Tok::Comma || k == Tok::Semicolon ||
                k == Tok::RBracket || k == Tok::RBrace || k == Tok::Eof)
              break;
            ++i;
          }
          if (i < toks.size() && toks[i].k == Tok::RParen) ++i;
          return;
        }

        ++i; // consume ')'
        return;
      }

      case Tok::Int: {
        this->emit_u64(PUSH_I);
        this->emit_s64(std::stoll(t().text.c_str()));
        ++i;
        return;
      }

      case Tok::Float: {
        this->emit_u64(PUSH_F);
        double f = std::stod(t().text.c_str());
        this->emit_f64(f);
        ++i;
        return;
      }

      case Tok::Str: {
        this->emit_u64(PUSH_S);
        this->emit_str(t().text);
        ++i;
        return;
      }

      case Tok::True:
      case Tok::False: {
        this->emit_u64(PUSH_B);
        this->emit_u64(t().k == Tok::True ? 1 : 0);
        ++i;
        return;
      }

      case Tok::Null: {
        this->emit_u64(PUSH_N);
        ++i;
        return;
      }

      case Tok::Id: {
        this->emit_u64(GET);
        this->emit_str(t().text);
        ++i;
        return;
      }

      case Tok::Minus: {
        ++i;
        Factor();
        this->emit_u64(NEG);
        return;
      }

      // --- Logical not
      case Tok::NE: {
        ++i;
        Factor();
        this->emit_u64(NOT);
        return;
      }

      // --- Error / unexpected token
      default: {
        ERR(getCurrentLoc(), "Expected value, identifier, or '(' expression ')'");
        // Recovery: skip to next likely resync token
        while (i < toks.size()) {
          Tok k = toks[i].k;
          if (k == Tok::RParen || k == Tok::Comma || k == Tok::Semicolon ||
              k == Tok::RBracket || k == Tok::RBrace || k == Tok::Eof)
            break;
          ++i;
        }
        if (i < toks.size()) ++i;
        return;
      }
    }
  }

  // ---- Statements -> bytecode ----
  struct LoopLbl {
    uint64_t condOff=0, contTarget=0;
    std::vector<uint64_t> breakPatchSites;
  };
  std::vector<LoopLbl> loopStack;

  inline void patchJump(uint64_t at, uint64_t target){ auto cur=tell(); seek(at); write_u64(out,target); seek(cur); }

  inline void StmtSeq(){
    while(i < toks.size()){
      if (toks[i].k == Tok::RBrace || toks[i].k == Tok::Eof) break;

      if (toks[i].k == Tok::LBrace) {
        ++i;
        StmtSeqUntilBrace();
        continue;
      }
      else if(toks[i].k == Tok::Exit){
        ++i;
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        this->emit_u64(HALT);
        continue;
      }

      else if(toks[i].k == Tok::Import){
        ++i;
        // Skip import target
        if (i < toks.size() && (toks[i].k == Tok::Str || toks[i].k == Tok::Id)) ++i;
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        continue;
      }

      else if(toks[i].k == Tok::Del){
        ++i;
        if (i >= toks.size() || toks[i].k != Tok::Id) {
          ERR(getCurrentLoc(), "Expected identifier after 'del'");
          continue;
        }
        CString name = toks[i].text;
        ++i;
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        this->emit_u64(UNSET);
        this->emit_str(name);
        continue;
      }

      else if(toks[i].k == Tok::Return){
        ++i;
        if (i < toks.size() && toks[i].k == Tok::Semicolon) {
          ++i;
          this->emit_u64(RET_VOID);
          continue;
        }
        Expr();
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        this->emit_u64(RET);
        continue;
      }

      else if(toks[i].k == Tok::PP) {
        ++i;
        if (i >= toks.size() || toks[i].k != Tok::Id) {
          ERR(getCurrentLoc(), "Expected identifier after '++'");
          continue;
        }
        CString name = toks[i].text;
        ++i;
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        this->emit_u64(PUSH_I);
        this->emit_s64(1);
        this->emit_u64(ADD);
        this->emit_u64(SET);
        this->emit_str(name);
        continue;
      }

      else if(toks[i].k == Tok::Cont){
        ++i;
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        if(this->loopStack.empty()) {
          ERR(getCurrentLoc(), "'continue' outside of loop");
          continue;
        }
        this->emit_u64(JMP);
        this->emit_u64(loopStack.back().contTarget);
        continue;
      }

      else if(toks[i].k == Tok::Break){
        ++i;
        size_t levels=1;
        if(i < toks.size() && toks[i].k == Tok::Int){
          levels = (size_t)std::stoll(toks[i].text.c_str());
          ++i;
        }
        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
        if(this->loopStack.size()<levels) {
          ERR(getCurrentLoc(), "'break' outside of loop");
          continue;
        }
        size_t idx=loopStack.size()-levels;
        this->emit_u64(JMP);
        auto at=tell();
        this->emit_u64(0);
        loopStack[idx].breakPatchSites.push_back(at);
        continue;
      }

      else if(toks[i].k == Tok::Func) {
        ++i;

        // Parse function attributes now as keywords
        bool isInline = false;
        bool tailCallOpt = false;

        if (i < toks.size() && toks[i].k == Tok::Inline) {
          ++i;
          isInline = true;
        }
        if (i < toks.size() && toks[i].k == Tok::Tail) {
          ++i;
          tailCallOpt = true;
        }

        // Track if types were specified for warning
        bool hasExplicitTypes = false;
        bool isVoid = false, typed = false;
        Type rt = Type::Int;
        CString fname;

        // Return type parsing with better error messages
        if (i < toks.size() && (toks[i].k == Tok::Void || toks[i].k == Tok::Int ||
            toks[i].k == Tok::Float || toks[i].k == Tok::Bool ||
            toks[i].k == Tok::Str || toks[i].k == Tok::List)) {
          hasExplicitTypes = true;
          if (toks[i].k == Tok::Void) {
            ++i;
            isVoid = true;
          } else {
            rt = parseType();
            ++i;
            typed = true;
          }
        }

        if (i >= toks.size() || toks[i].k != Tok::Id) {
          ERR(getCurrentLoc(), "Expected function name");
          continue;
        }
        fname = toks[i].text;
        ++i;

        // Parameter parsing with Python-style defaults
        if (i >= toks.size() || toks[i].k != Tok::LParen) {
          ERR(getCurrentLoc(), "Expected '(' after function name");
          continue;
        }
        ++i; // consume '('

        std::vector<CString> params;
        std::vector<std::pair<Type, std::optional<Value>>> paramTypes; // Track param types & defaults

        if(i < toks.size() && toks[i].k != Tok::RParen) {
          while (true) {
            // Check for type annotation
            Type paramType = Type::Int; // Default type if none specified
            if (i < toks.size() && (toks[i].k == Tok::Int || toks[i].k == Tok::Float ||
                toks[i].k == Tok::Bool || toks[i].k == Tok::Str ||
                toks[i].k == Tok::List)) {
              paramType = parseType();
              ++i;
              hasExplicitTypes = true;
            }

            if (i >= toks.size() || toks[i].k != Tok::Id) {
              ERR(getCurrentLoc(), "Expected parameter name");
              break;
            }
            params.push_back(toks[i].text);
            ++i;

            // Handle default value
            std::optional<Value> defaultVal;
            if (i < toks.size() && toks[i].k == Tok::Equal) {
              ++i;
              // Parse literal value for default
              if (i < toks.size()) {
                if (toks[i].k == Tok::Str) {
                  defaultVal = Value::S(toks[i].text.c_str());
                  ++i;
                } else if (toks[i].k == Tok::Int) {
                  defaultVal = Value::I(std::stoll(toks[i].text.c_str()));
                  ++i;
                } else if (toks[i].k == Tok::Float) {
                  defaultVal = Value::F(std::stod(toks[i].text.c_str()));
                  ++i;
                } else if (toks[i].k == Tok::True) {
                  defaultVal = Value::B(true);
                  ++i;
                } else if (toks[i].k == Tok::False) {
                  defaultVal = Value::B(false);
                  ++i;
                }
              }
            }

            paramTypes.push_back({paramType, defaultVal});

            if (i >= toks.size()) break;
            if (toks[i].k == Tok::RParen) break;
            if (toks[i].k == Tok::Comma) {
              ++i;
              continue;
            }
            ERR(getCurrentLoc(), "Expected ',' or ')' in parameter list");
            break;
          }
        }

        if (i < toks.size() && toks[i].k == Tok::RParen) ++i;

        // Emit type usage warning if needed
        if (!hasExplicitTypes) {
          std::cerr << "Warning: Function '" << fname.c_str() << "' uses implicit types. "
           << "Consider adding explicit type annotations for better safety and clarity.\n";
        }

        if (i >= toks.size() || toks[i].k != Tok::LBrace) {
          ERR(getCurrentLoc(), "Expected '{' after function declaration");
          continue;
        }
        ++i; // consume '{'

        // Register function
        CompilerFnInfo fni{fname, 0, params, isVoid, typed, rt};
        fni.isInline = isInline;
        fni.tail = tailCallOpt;
        fni.param_types = paramTypes;
        size_t idx = fns.size();
        fns.push_back(fni);
        fnIndex[fname] = idx;

        // Function body parsing
        this->emit_u64(JMP);
        auto skipAt = tell();
        this->emit_u64(0);

        auto entry = tell();
        fns[idx].entry = entry;

        StmtSeqUntilBrace();

        if (isVoid) this->emit_u64(RET_VOID);
        else this->emit_u64(RET);

        auto after = tell();
        patchJump(skipAt, after);
        continue;
      }

      // while (cond) { ... }
      else if (toks[i].k == Tok::While) {
        ++i;
        if (i >= toks.size() || toks[i].k != Tok::LParen) {
          ERR(getCurrentLoc(), "Expected '(' after 'while'");
          continue;
        }
        ++i; // consume '('

        auto condOff = tell();
        Expr();

        if (i >= toks.size() || toks[i].k != Tok::RParen) {
          ERR(getCurrentLoc(), "Expected ')' after while condition");
          continue;
        }
        ++i; // consume ')'

        this->emit_u64(JF);
        auto jfAt = tell();
        this->emit_u64(0);

        if (i >= toks.size() || toks[i].k != Tok::LBrace) {
          ERR(getCurrentLoc(), "Expected '{' after while condition");
          continue;
        }
        ++i; // consume '{'

        // Track loop labels for break/continue
        LoopLbl L{};
        L.condOff = condOff;
        L.contTarget = condOff;
        this->loopStack.push_back(L);

        // Enforce: at most one 'with' group inside this while
        bool thisWhileHasWith = false;

        StmtSeqUntilBrace();

        // loop back to condition
        this->emit_u64(JMP);
        this->emit_u64(condOff);

        // patch exits/breaks
        auto after = tell();
        patchJump(jfAt, after);
        for (auto site : loopStack.back().breakPatchSites) patchJump(site, after);
        loopStack.pop_back();

        continue;
      }

      // if (...) { ... } elif (...) { ... } else { ... }
      else if(toks[i].k == Tok::If){
        ++i;
        if (i >= toks.size() || toks[i].k != Tok::LParen) {
          ERR(getCurrentLoc(), "Expected '(' after 'if'");
          continue;
        }
        ++i; // consume '('

        Expr();

        if (i >= toks.size() || toks[i].k != Tok::RParen) {
          ERR(getCurrentLoc(), "Expected ')' after if condition");
          continue;
        }
        ++i; // consume ')'

        this->emit_u64(JF);
        auto jfAt = tell();
        this->emit_u64(0);

        if (i >= toks.size() || toks[i].k != Tok::LBrace) {
          ERR(getCurrentLoc(), "Expected '{' after if condition");
          continue;
        }
        ++i; // consume '{'

        StmtSeqUntilBrace();

        this->emit_u64(JMP);
        auto jendAt = tell();
        this->emit_u64(0);
        auto afterThen = tell();
        patchJump(jfAt, afterThen);

        std::vector<uint64_t> ends;
        ends.push_back(jendAt);

        while(i < toks.size() && toks[i].k == Tok::Elif){
          ++i; // consume 'elif'
          if (i >= toks.size() || toks[i].k != Tok::LParen) {
            ERR(getCurrentLoc(), "Expected '(' after 'elif'");
            break;
          }
          ++i; // consume '('

          Expr();

          if (i >= toks.size() || toks[i].k != Tok::RParen) {
            ERR(getCurrentLoc(), "Expected ')' after elif condition");
            break;
          }
          ++i; // consume ')'

          this->emit_u64(JF);
          auto ejf = tell();
          this->emit_u64(0);

          if (i >= toks.size() || toks[i].k != Tok::LBrace) {
            ERR(getCurrentLoc(), "Expected '{' after elif condition");
            break;
          }
          ++i; // consume '{'

          StmtSeqUntilBrace();
          this->emit_u64(JMP);
          auto ejend=tell();
          this->emit_u64(0);
          ends.push_back(ejend);
          auto afterElif=tell();
          patchJump(ejf, afterElif);
        }

        if(i < toks.size() && toks[i].k == Tok::Else){
          ++i; // consume 'else'
          if (i >= toks.size() || toks[i].k != Tok::LBrace) {
            ERR(getCurrentLoc(), "Expected '{' after 'else'");
          } else {
            ++i; // consume '{'
            StmtSeqUntilBrace();
          }
        }
        auto afterAll=tell();
        for(auto at: ends) patchJump(at, afterAll);
        continue;
      }

      // let [type|auto] name = expr ;
      else if(toks[i].k == Tok::Let){
        ++i; // consume 'let'

        // Parse modifiers
        bool isConst = false;
        bool isStatic = false;
        bool DList = false;

        if (i < toks.size() && toks[i].k == Tok::Const) {
          isConst = true;
          ++i;
        }

        if (i < toks.size() && toks[i].k == Tok::Static) {
          isStatic = true;
          ++i;
        }

        if (i < toks.size() && toks[i].k == Tok::Dead) {
          DList = true;
          ++i;
        }

        // Parse type
        bool isAuto=false;
        bool isNull=false;
        Type T=Type::Int;

        if(i < toks.size() && toks[i].k == Tok::Auto) {
          isAuto=true;
          ++i;
        }
        else if(i < toks.size() && toks[i].k == Tok::Null) {
          isNull=true;
          ++i;
        }
        else if (i < toks.size() && (toks[i].k == Tok::Int || toks[i].k == Tok::Float ||
                 toks[i].k == Tok::Bool || toks[i].k == Tok::Str ||
                 toks[i].k == Tok::List)) {
          T = parseType();
          ++i;
        }

        if (i >= toks.size() || toks[i].k != Tok::Id) {
          ERR(getCurrentLoc(), "Expected variable name");
          continue;
        }
        CString name = toks[i].text;
        ++i;

        // Handle initialization
        if(!isNull && !DList) {
          if (i >= toks.size() || toks[i].k != Tok::Equal) {
            ERR(getCurrentLoc(), "Expected '=' for variable initialization");
            continue;
          }
          ++i; // consume '='
          Expr();
        }

        // Expect semicolon
        if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;

        // Encode modifiers in high bits of type byte
        uint64_t type_byte = isAuto ? 0xEC : (uint8_t)T;
        if(isConst) type_byte |= 0x100;
        if(isStatic) type_byte |= 0x200;

        this->emit_u64(DECL);
        this->emit_str(name);
        this->emit_u64(type_byte);
        continue;
      }

      // assignment or call;
      else if(i < toks.size() && toks[i].k == Tok::Id){
        size_t save=i;
        CString name = toks[i].text;
        ++i;

        if(i < toks.size() && toks[i].k == Tok::Equal){
          ++i; // consume '='
          Expr();
          // Expect semicolon
          if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
          this->emit_u64(SET);
          this->emit_str(name);
          continue;
        } else {
          i=save;
          Expr();
          // Expect semicolon
          if (i < toks.size() && toks[i].k == Tok::Semicolon) ++i;
          this->emit_u64(POP);
          continue;
        }
      }

      else {
        ERR(getCurrentLoc(), "unexpected token");
        ++i; // Skip problematic token
      }
    }
  }

  inline void StmtSeqOne(){
    if(i >= toks.size() || toks[i].k == Tok::RBrace || toks[i].k == Tok::Eof) return;
    StmtSeq();
  }

  inline void StmtSeqUntilBrace(){
    size_t depth=1;
    while(i < toks.size()){
      if(toks[i].k == Tok::RBrace){
        --depth;
        ++i;
        if(depth==0) break;
        continue;
      }
      if(toks[i].k == Tok::LBrace){
        ++depth;
        ++i;
        continue;
      }
      StmtSeqOne();
    }
  }

  inline void writeHeaderPlaceholders(){
    fwrite("AVOCADO1",1,8,out);
    table_offset_pos = (uint64_t)ftell(out); write_u64(out, 0); // table_offset
    fn_count_pos     = (uint64_t)ftell(out); write_u64(out, 0); // count
    entry_main_pos   = (uint64_t)ftell(out); write_u64(out, 0); // entry_main
  }

  inline void compileToFile(const CString& outPath) {
    out = fopen(outPath.c_str(), "wb+");
    if (!out) throw std::runtime_error("cannot open bytecode file for write");

    writeHeaderPlaceholders();

    // Top-level as __main__
    CompilerFnInfo mainFn{cstr("__main__"), 0, {}, true, false, Type::Int};
    fns.push_back(mainFn);
    fnIndex[cstr("__main__")] = 0;
    fns[0].entry = tell();

    i = 0;
    StmtSeq();
    this->emit_u64(HALT);

    // Write function table
    uint64_t tbl_off = tell();
    uint64_t count = (uint64_t)fns.size();

    for (auto& fn : fns) {
      write_str(out, fn.name.c_str());
      write_u64(out, fn.entry);
      write_u8(out, fn.isVoid ? 1 : 0);
      write_u8(out, fn.typed ? 1 : 0);
      write_u8(out, (uint8_t)fn.ret);
      write_u64(out, (uint64_t)fn.params.size());
      for (const auto& s : fn.params) {
        write_str(out, s.c_str());
      }
    }

    // Patch header
    fflush(out);
    seek(table_offset_pos);
    write_u64(out, tbl_off);
    seek(fn_count_pos);
    write_u64(out, count);
    seek(entry_main_pos);
    write_u64(out, fns[0].entry);

    fclose(out);
    out = nullptr;
  }
};
