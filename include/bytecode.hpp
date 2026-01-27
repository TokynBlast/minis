#pragma once
#include <cstdint>
#include "macros.h"

// NOTE: 3-bit register and 5-bit opcode
enum class Register : uint8 {
  LOGIC,    // Boolean and comparison operations
  VARIABLE, // Operations on variables
  FUNCTION, // Function support
  IMPORT,   // Importing libraries and other files
  GENERAL,  // Operations that don't fit into the others
  MATH,     // Math operations (Powered by Fortran)
  STACK,    // Stack manipulation
  BITWISE   // Bitwise operations
};

enum class Import : uint8 {
  FUNC,     // Run external function
  LOAD,     // Load file/module
  PLUGIN    // Load plugin
};

enum class Variable : uint8 {
  GET,      // Get variable -> push to stack
  SET,      // Pop stack -> set variable
  DECLARE,  // Declare variable with no value
  UNSET,    // Delete a variable
  PUSH      // Push literal to stack
};

enum class Logic : uint8 {
  EQUAL,         // Pop b,a -> push (a == b)
  NOT_EQUAL,     // Pop b,a -> push (a != b)
  LESS_OR_EQUAL, // Pop b,a -> push (a <= b)
  LESS_THAN,     // Pop b,a -> push (a < b)
  AND,           // Pop b,a -> push (a && b)
  OR,            // Pop b,a -> push (a || b)
  JUMP,          // Jump to address unconditionally
  JUMP_IF_NOT,   // Pop cond, jump if false
  NOT,            // Pop a -> push (!a)
  JUMP_IF   // Pop cond, jump if true
};

enum class Func : uint8 {
  CALL,       // Call a function by name
  TAIL,       // Tail call optimization
  RETURN,     // Return value(s) from function
  BUILTIN,    // Call builtin function
};

enum class Math : uint8 {
  ADD,        // Pop b,a -> push (a + b)
  SUB,        // Pop b,a -> push (a - b)
  MULT,       // Pop b,a -> push (a * b)
  DIV,        // Pop b,a -> push (a / b)
  ADD_MULT,   // Add N values (Fortran vectorized)
  DIV_MULT,   // Divide N values (Fortran vectorized)
  MULT_MULT,  // Multiply N values (Fortran vectorized)
  SUB_MULT,   // Subtract N values (Fortran vectorized)
  MOD,        // Pop b,a -> push (a % b)
  POW         // Pop b,a -> push (a^b)
};

enum class General : uint8 {
  HALT,       // Stop program
  NOP,        // Do nothing
  POP,        // Pop and discard top of stack
  INDEX,      // Pop list,idx -> push list[idx]
  YIELD       // Wait for enter key
};

enum class Stack : uint8 {
  DUP,        // Duplicate top of stack
  SWAP,       // Swap top two stack values
  ROT,        // Rotate top 3 (a b c -> b c a)
  OVER,       // Copy second item to top (a b -> a b a)
  ASSERT      // Pop cond, halt if false
};

enum class Bitwise : uint8 {
  AND,  OR,   XOR,  NOT,
  SHL,  SHR,  ROL,  ROR
};
