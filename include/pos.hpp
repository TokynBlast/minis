#pragma once
#include <cstdint>
#include "sso.hpp"

struct Pos{
  uint32_t line=1,  col=1;
  lang::CString src;

  Pos() = default;
};