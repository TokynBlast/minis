#pragma once
#include <memory>
#include <vector>
#include "token.hpp"

namespace minis{
  struct Stmt; struct Program;

  std::unique_ptr<Program> parseProgram(const std::vector<Token>& toks);
}