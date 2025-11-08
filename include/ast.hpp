#pragma once
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include "types.hpp"

namespace lang{
  struct Param { std::string name; Type type = Type::Int; };
  struct Expr { virtual ~Expr() = default; Type type=Type::UNKNOWN; };
  struct Stmt { virtual ~Stmt() = default; };
  struct ident   : Expr { std::string name; };
  struct Call    : Expr { std::string fn; std::vector<std::unique_ptr<Expr>> args; };

  struct Expr    : Stmt { std::unique_ptr<Expr> e; };
  struct Decl    : Stmt {
    std::string name;
    bool isAuto=false, isNull=false;
    Type declared=Type::Int;
    bool isConst=false, isStatic=false; // const/static
    std::unique_ptr<Expr> init; // null if isNull
  };
  struct SAssign  : Stmt { std::string name; std::unique_ptr<Expr> rhs; };
  struct SDel     : Stmt { std::string name; };
  // FIXME: SConv should be (int)x, to convert x to int and so on
  struct Conv    : Stmt { std::string name; Type to; };
  struct Return  : Stmt { std::unique_ptr<Expr> value; bool isVoid=false; };
  struct Break   : Stmt { size_t level = 1; };
  struct Cont    : Stmt {};
  struct Yield   : Stmt {};

  struct WS       : Stmt { size_t size = 0; };

  struct Throw   : Stmt { std::string typeName; std::optional<std::string> msg; };
  
  struct Block   : Stmt { std::vector<std::unique_ptr<Stmt>> stmts; };
  
  struct If      : Stmt { 
    struct Arm { std::unique_ptr<Expr> cond; std::unique_ptr<SBlock> body; };
    std::vector<Arm> arms;
  };

  struct SWhile   : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<SBlock> body;
    //FIXME: add with/and blocks
    std::vector<std::unique_ptr<SBlock>> withBlocks;
  };

  struct SFunc    : Stmt {
    std::string name;
    bool isVoid=false, hasExplicitRet=false;
    Type ret = Type::Int;
    std::vector<Param> params;
    std::unique_ptr<SBlock> body;
    //FIXME: add quality
  }

  struct Program : Node { std::vector<std::unique_ptr<Stmt>> items; };
}