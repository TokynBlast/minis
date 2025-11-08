#pragma once
#include <memory>
#include <vector>
#include <string>
#include "types.hpp"

namespace lang{
  struct Expr { virtual ~Expr() = default; Type type=Type::UNKNOWN; };
  struct Stmt { virtual ~Stmt() = default; };
  struct ELit     : Expr {/* Value val; */};
  struct Eident   : Expr { std::string name; };
  struct ECall    : Expr { std::string fn; std::vector<std::unique_ptr<Expr>> args; };

  struct SExpr    : Stmt { std::unique_ptr<Expr> e; };
  struct SDecl    : Stmt {
    std::string name;
    bool isAuto=false, isNull=false;
    Type declared=Type::Int;
    Qual quals=Qual::None; // const/static
    std::unique_ptr<Expr> init; // null if isNull
  };
  struct SAssign  : Stmt { std::string name; std::unique_ptr<Expr> rhs; };
  struct SDel     : Stmt { std::string name; };
  // FIXME: SConv should be (int)x, to convert x to int and so on
  struct SConv    : Stmt { std::string name; Type to; };
  struct SReturn  : Stmt { std::unique_ptr<Expr> value; bool isVoid=false; };
  struct SBreak   : Stmt { size_t level = 1; };
  struct SCont    : Stmt {};
  struct SYield   : Stmt {};

  struct SThrow   : Stmt { std::string typeName; std::optional<std::string> msg; };
  
  struct SBlock   : Stmt { std::vector<std::unique_ptr<Stmt>> stmts; };
  
  struct SIf      : Stmt { 
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
    Attr attrs = Attr::None;
    std::vector<Param> params;
    std::unique_ptr<SBlock> body;
    //FIXME: add quality
  }

  struct Program : Node { std::vector<std::unique_ptr<Stmt>> items; };
}