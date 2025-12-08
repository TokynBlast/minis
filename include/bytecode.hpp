#pragma once
#include <cstdint>
enum Op : std::uint16_t {
  // importing
  IMPORT_REGISTER = 0x0A,
  IMPORT_FUNC  = 0x01,
  IMPORT_LOAD = 0x02,
  IMPORT_STORE = 0x03,

  // making variables
  PUSH_REGISTER = 0x0B,
  PUSH_INT = 0x04,
  PUSH_FLOAT = 0x05,
  PUSH_BOOL = 0x06,
  PUSH_STRING = 0x07,
  PUSH_C = 0x08,
  PUSH_NULL = 0x09,
  MAKE_LIST = 0x010,

  // work with variables :3
  VARIABLE = 0x0C,
  GET = 0x,
  SET = 0x4D,
  DECL = ,
  POP,            // 0F

  // math opertions
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
  NO_OP = 0x3C,
};
