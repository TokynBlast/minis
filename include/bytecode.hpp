#pragma once
#include <cstdint>
#include "macros.h"

// NOTE: We may do 3-bit register and 5-bit opcode
enum class Register : uint8 {
  IMPORT = 0, // Importing libraries and other files
  VARIABLE,   // Operations on variables
  LOGIC,      // Math and boolean logics
  FUNCTION,   // Function support
  GENERAL,    // Operations that don't fit into the others
  MATH        // Math operations (Powered by Fortran)
};

enum class Import : uint8 {
  FUNC = 0, // Load in and run a function
  LOAD,     // ???
  STORE     // ???
};

enum class Variable : uint8 {
  GET = 0,  // Get a varable
  SET,      // Set a variable
  DECLARE,  // Declare a variable with no value
  UNSET,    // Delete a variable
  PUSH,     // Push a variable to the current stack
  UNSET_ALL // Delete all variables
};

enum class Logic : uint8 {
  EQUAL = 0,     // True if values are equal
  NOT_EQUAL,     // True if values are not equal
                 // NOTE: the following replace greater than and greater than or equal to
  LESS_OR_EQUAL, // True if left is less than or equal to right
  LESS_THAN,     // True if left is less but not equal to right
  AND,           // True if left and right are true
  OR,            // True if left or right are true
  JUMP,          // Jump to location
  JUMP_IF,       // Jump if true
  JUMP_IF_FALSE, // Jump if false
};

// FIXME: We should use return, and make it optional :)
enum class Func : uint8 {
  CALL = 0,   // Call a function
  TAIL,       // Clear and reuse stack
  RETURN,     // Return any amount of values
};

enum class Math : uint8_t {
  ADD,       // Add to variable
  SUB,       // Subtract from variable
  MULT,      // Multiply a variable
  DIV,       // Divide a variable
  NEGATE,    // Flip a variable's signedness
  ADD_MULT,  // Add multiple values
  DIV_MULT,  // Divide multiple values
  MULT_MULT, // Multiply multiple values
  SUB_MULT   // Subtract multiple values
};

enum class General : uint8 {
  HALT = 0,  // Stop program
  SLICE,     // ???
  SET_INDEX, // ???
  NOP,       // Do nothing
  POP,       // Pop current value on top of stack
  INDEX,     // Split a list or string
  EMIT,      // ???
  YIELD      // Wait for enter key
};
