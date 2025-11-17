#pragma once
#include <cstdint>
enum Op : std::uint16_t {
  IMPORTED_FUNC  = 0x01,
  IMPORTED_LOAD,  // 02
  IMPORTED_STORE, // 03
  NOP,            // 04
  PUSH_I,         // 05
  PUSH_F,         // 06
  PUSH_B,         // 07
  PUSH_S,         // 08
  PUSH_C,         // 09
  PUSH_N,         // 0A
  MAKE_LIST,      // 0B
  GET,            // 0C
  SET,            // 0D
  DECL,           // 0E
  POP,            // 0F
  ADD,            // 10
  SUB,            // 11
  MUL,            // 12
  DIV,            // 13
  NEG,            // 14
  EQ,             // 15
  NE,             // 16
  LT,             // 17
  LE,             // 18
  AND,            // 19
  OR,             // 1A
  JMP,            // 1B
  JF,             // 1C
  CALL,           // 1D
  RET,            // 1E
  RET_VOID,       // 1F
  HALT,           // 20
  UNSET,          // 21
  SLICE,          // 22
  INDEX,          // 23
  SET_INDEX,      // 24
  TAIL,           // 25
  YIELD,          // 26
  NOT             // 27
};
