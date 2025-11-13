#pragma once

namespace lang {
  enum class Type : unsigned char {
    Int=0, Float=1, Bool=2, Str=3, List=4, Null=5,
  };

  inline const char* TypeName(Type t){
    switch(t){
      case Type::Int: return "int";
      case Type::Float: return "float";
      case Type::Bool: return "bool";
      case Type::Str: return "str";
      case Type::List: return "list";
      case Type::Null: return "null";
    }
    return "?";
  }
}