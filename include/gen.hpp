#pragma once
#include <cstdio>
#include "ast.hpp"

namespace minis {
  struct gen {
    FILE* out = nullptr;
    explicit Codegen(FILE* f): out(f) {}
    void run(Program& p); // AST -> bytecode
  };
}