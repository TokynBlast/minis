#pragma once

#include <vector>
#include "token.hpp"

namespace lang {
  char* Ugly(const std::vector<Token>& tokens);

  bool is_builtin(const char* id);
}