#pragma once
#include <cstdint>
#include "macros.hpp"

// Up to 240 opcodes

enum class Register : uint8 {
  IMPORT = 0,
  VARIABLE,
  LOGIC,
  FUNCTION,
  GENERAL
};

enum class Import : uint8 {
  FUNC = 0,
  LOAD,
  STORE
};

enum class Variable : uint8 {
  GET = 0,
  SET,
  DECLARE,
  UNSET,
  PUSH
};

enum class Math : uint8 {
  ADD = 0,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  NEGATE
};

enum class Logic : uint8 {
  EQUAL = 0,
  NOT_EQUAL,
  LESS_THAN_OR_EQUAL,
  LESS_THAN,
  AND,
  OR,
  JUMP,
  JUMP_IF
};

enum class Func : uint8 {
  CALL = 0,
  TAIL,
  RETURN,
  RETURN_VOID
};

enum class General : uint8 {
  HALT = 0,
  SLICE,
  SET_INDEX,
  NO_OP,
  POP,
  INDEX
};
