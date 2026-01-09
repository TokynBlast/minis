#pragma once

namespace minis {
  enum class Type : unsigned char {
    Int=0,
    Float,
    Bool,
    Str,
    List,
    Dict,
    i8,
    i16,
    i32,
    i64,
    ui8,
    ui16,
    ui32,
    ui64,
    Null,
    Range
  };

  // FIXME: We should use the class above over a switch.
  inline const char* TypeName(Type t){
    switch(t){
      case Type::Int: return "int"; // Let the compiler choose bit size
      case Type::Float: return "float";
      case Type::Bool: return "bool";
      case Type::Str: return "str";
      case Type::List: return "list";
      case Type::Dict: return "dict";
      case Type::i8: return "i8"; // 8-bit signed
      case Type::i16: return "i16"; // 16-bit signed
      case Type::i32: return "i32"; // 32-bit signed
      case Type::i64: return "i64"; // 64-bit signed
      case Type::ui8: return "ui8"; //
      case Type::ui16: return "ui16";
      case Type::ui32: return "ui32";
      case Type::ui64: return "ui64";
      case Type::Null: return "null";
      case Type::Range: return "range";
    }
    return "?";
  }
}
