#pragma once
#include <cstdint>

constexpr enum class Register : std::uint8_t {
  IMPORT = 0x0A,
  VARIABLE = 0x0B,
  LOGIC = 0x0C,
  FUNCTION = 0x0D,
  GENERAL = 0x0E,
}

constexpr enum class Import : std::uint8_t {
  FUNC  = 0x01,
  LOAD,
  STORE
}

constexpr enum class Variable : std::uint8_t {
  GET = 0x01,
  SET,
  DECLARE,
  UNSET,
  PUSH
}

constexpr enum class Math : std::uint8_t {
  ADD = 0x01,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  NEGATE
}

constexpr enum class Logic : std::uint8_t
  EQUAL = 0x01,
  NOT_EQUAL,
  LESS_THAN_OR_EQUAL,
  LESS_THAN,
  AND,
  OR,
  JUMP,
  JUMP_IF
}

constexpr enum class Func : std::uint8_t
  CALL = 0x01,
  TAIL,
  RETURN,
  RETURN_VOID,
}

constexpr enum class General : std::uint8_t
  HALT = 0x01,
  SLICE,
  SET_INDEX,
  NO_OP,
  POP,
  INDEX
};
