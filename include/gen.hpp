#pragma once
#include <cstdio>
#include "ast.hpp"

namespace lang {
  struct gen {
    FILE* out = nullptr;
    explicit gen(FILE* f): out(f) {}
    void run(Program& p); // AST -> bytecode
  };
}