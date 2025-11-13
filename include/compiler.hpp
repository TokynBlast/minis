#pragma once
#include <vector>
#include <unordered_map>
#include <optional>
#include "sso.hpp"
#include "token.hpp"
#include "types.hpp"
#include "value.hpp"

namespace lang {
  struct CompilerFnInfo {
    CString name;
    uint64_t entry = 0;
    std::vector<CString> params;
    bool isVoid = false;
    bool typed = false;
    Type ret = Type::Int;
    bool isInline = false;
    bool tail = false;
    std::vector<std::pair<Type, std::optional<Value>>> param_types;
  };

  struct Compiler {
    FILE* out = nullptr;
    std::vector<CompilerFnInfo> fns;
    std::unordered_map<CString, size_t> fnIndex;
    bool inWith;

    std::vector<Token> toks;
    size_t i = 0;

    uint64_t table_offset_pos = 0, fn_count_pos = 0, entry_main_pos = 0;

    explicit Compiler(const std::vector<Token>& tokens);

    const Token& t() const;
    inline Loc getCurrentLoc() const;
    inline Type parseType();

    // Bytecode emission
    inline void emit_u8(uint8_t v);
    inline void emit_u64(uint64_t v);
    inline void emit_s64(int64_t v);
    inline void emit_f64(double v);
    inline void emit_str(const CString& s);
    inline uint64_t tell() const;
    inline void seek(uint64_t pos);

    // Expression parsing
    inline void Expr();
    inline void LogicOr();
    inline void LogicAnd();
    inline void Equality();
    inline void AddSub();
    inline void MulDiv();
    inline void ListLit();
    inline void Factor();

    // Statement parsing
    struct LoopLbl {
      uint64_t condOff = 0, contTarget = 0;
      std::vector<uint64_t> breakPatchSites;
    };
    std::vector<LoopLbl> loopStack;

    inline void patchJump(uint64_t at, uint64_t target);
    inline void StmtSeq();
    inline void StmtSeqOne();
    inline void StmtSeqUntilBrace();
    inline void writeHeaderPlaceholders();

    // Main compilation entry point
    inline void compileToFile(const CString& outPath);
  };
}