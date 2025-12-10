#pragma once

namespace lang {
  enum class Type : unsigned char {
    Int=0, Float, Bool, Str, List, Dict, i8, i16, i32, i63, Null
  };

  inline const char* TypeName(Type t){
    switch(t){
      case Type::Int: return "int"; // Let the compiler choose bit size
      case Type::Float: return "float";
      case Type::Bool: return "bool";
      case Type::Str: return "str";
      case Type::List: return "list";
      case Type::Dict: return "dict";
      case Type::i8: return "i8"; // 8-bit
      case Type::i16: return "i16"; // 16-bit
      case Type::i32: return "i32"; // 32-bit
      case Type::i64: return "i64"; // 64-bit
      case Type::Null: return "null";
    }
    return "?";
  }
}
