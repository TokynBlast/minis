#pragma once
#include <memory>
#include <vector>
#include <optional>
#include "types.hpp"
#include "sso.hpp"

namespace lang{
  struct Param { CString name; Type type = Type::Int; };
  struct Expr { virtual ~Expr() = default; Type type=Type::Int; };
  struct Stmt { virtual ~Stmt() = default; size_t s = 0; }; // size :)
  struct ident   : Expr { CString name; };
  struct Call    : Expr { CString fn; std::vector<std::unique_ptr<Expr>> args; };

  struct Decl    : Stmt {
    CString name;
    bool isAuto=false, isNull=false;
    Type declared=Type::Int;
    bool isConst=false, isStatic=false; // const/static
    std::unique_ptr<Expr> init; // null if isNull
  };
  struct SAssign  : Stmt { CString name; std::unique_ptr<Expr> rhs; };
  struct SDel     : Stmt { CString name; };
  // FIXME: SConv should be (int)x, to convert x to int and so on
  struct Conv    : Stmt { CString name; Type to; size_t s = 4; };
  struct Return  : Stmt { std::unique_ptr<Expr> value; bool isVoid=false; size_t s = 6; };
  struct Break   : Stmt { size_t level = 1; size_t s = 5; };
  struct Cont    : Stmt { size_t s = 8; };
  struct Yield   : Stmt { size_t s = 5; };

  struct WS      : Stmt { size_t s = 0; int NL=0; };

  struct Throw   : Stmt { CString typeName; std::optional<CString> msg; size_t s = 5; };

  //FIXME: Not sure how or if this needs or gets a size?
  struct Block   : Stmt { std::vector<std::unique_ptr<Stmt>> stmts; size_t s = 1; };

  struct If      : Stmt {
    struct Arm { std::unique_ptr<Expr> cond; std::unique_ptr<Block> body; };
    std::vector<Arm> arms;
    size_t s = 2;
  };
  struct ElseIf  : Stmt {
    struct Arm { std::unique_ptr<Expr> cond; std::unique_ptr<Block> body; };
    std::vector<Arm> arms;
    size_t s = 4;
  };
  struct Else    : Stmt {
    struct Arm { std::unique_ptr<Expr> cond; std::unique_ptr<Block> body; };
    std::vector<Arm> arms;
    size_t s = 4;
  };

  struct While   : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Block> body;
    //FIXME: with and shouldn't be in while anymore
    std::vector<std::unique_ptr<Block>> withBlocks;
    size_t s = 5;
  };

  // exit, try, except, finally, lambda, import
  struct Func    : Stmt {
    CString name;
    bool isVoid=false, hasExplicitRet=false;
    Type ret = Type::Int;
    std::vector<Param> params;
    std::unique_ptr<Block> body;
    //FIXME: add quality
    size_t s = 4;
  };

  struct Program { std::vector<std::unique_ptr<Stmt>> items; };
}