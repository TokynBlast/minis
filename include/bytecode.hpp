#pragma once
#include <cstdint>

enum class Register : std::uint8_t {
  IMPORT = 1,
  VARIABLE,
  LOGIC,
  FUNCTION,
  GENERAL,
}

enum class Import : std::uint8_t {
  FUNC  = 1,
  LOAD,
  STORE
}

enum class Variable : std::uint8_t {
  GET = 1,
  SET,
  DECLARE,
  UNSET,
  PUSH
}

enum class Math : std::uint8_t {
  ADD = 1,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  NEGATE
}

enum class Logic : std::uint8_t
  EQUAL = 1,
  NOT_EQUAL,
  LESS_THAN_OR_EQUAL,
  LESS_THAN,
  AND,
  OR,
  JUMP,
  JUMP_IF
}

enum class Func : std::uint8_t
  CALL = 1,
  TAIL,
  RETURN,
  RETURN_VOID,
}
enum class General : std::uint8_t
  HALT = 1,
  SLICE,
  SET_INDEX,
  NO_OP,
  POP,
  INDEX
};
