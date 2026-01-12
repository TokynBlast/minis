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
}
