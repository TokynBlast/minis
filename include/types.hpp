#pragma once
#include <string>

namespace minis {
  enum class DType : unsigned char {
    Int=0, Float=1, Bool=2, Str=3, List=4, Null=5,
    ERR=250, UNKNOWN=251
  };

  inline const char* TypeName(DType t){
    switch(t){
      case DType::Int: return "int";
      case DType::Float: return "float";
      case DType::Bool: return "bool";
      case DType::Str: return "str";
      case DType::List: return "list";
      case DType::Null: return "null";

      case DType::ERR: return "<ERR>";
      case DType::UNKNOWN: return "<UNKNOWN>";
    }
    return "?";
  }
}