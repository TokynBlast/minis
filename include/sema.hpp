#pragma once
#include "ast.hpp"

namespace lang {
  struct Semantics {
    void run(Program& p);
  };
}