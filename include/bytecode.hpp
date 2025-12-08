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
    PUSH_LIST = 0x010,

  // work with variables :3
  VARIABLE_REGISTER = 0x0C,
    GET = 0x011,
    SET = 0x012,
    DECLARE = 0x013,
    UNSET = 0x15,

  // math opertions
  MATH_REGISTER = 0x0D
    ADD = 0x016,
    SUBTRACT = 0x017,
    MULTIPLY = 0x018,
    DIVIDE = 0x019,
    NEGATE = 0x020,

  LOGIC_REGISTER = 0x0E,
    EQUAL = 0x020,
    NOT_EQUAL = 0x21,
    LESS_THAN_OR_EQUAL = 0x022,
    LESS_THAN = 0x023,
    AND = 0x024,
    OR = 0x025,
    JUMP = 0x026,
    JUMP_IF = 0x027,
    NOT_EQUAL = 0x28,


  FUNCTION_REGISTER = 0x0F,
    CALL = 0x029,
    TAIL = 0x030,
    RETURN = 0x031,
    RETURN_VOID = 0x032,

  GENERAL_REGISTER = 0x01A
    HALT = 0x033,
    SLICE = 0x034,
    SET_INDEX = 0x035,
    NO_OP = 0x036,
    POP = 0x037,
    INDEX = 0x038,
};
