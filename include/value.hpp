#pragma once
#include "macros.hpp"
#include <variant>
#include <vector>
#include <map>
#include <sstream>
#include <stdint.h>
#include "types.hpp"
#include "sso.hpp"

using namespace minis;

struct Value
{
  Value() = default;

  Type t = Type::Null;
  std::variant<
      CString,                // Str
      int,                    // Int
      bool,                   // Bool
      std::vector<Value>,     // List
      std::map<Value, Value>, // dictionary
      std::map<int, int>,     // range
      double,                 // float
      std::monostate,         // null
      int8,                 // 8-bit int
      int16,                // 16-bit int
      int32,                // 32-bit int
      int64,                // 64-bit int
      uint8,                  // unsigned 8-bit int
      uint16,                 // unsigned 16-bit int
      uint32,                 // unsigned 32-bit
      uint64                  // unsigned 64-bit int
      >
      v;

  Value(Type type) : t(type) {}
  Value(Type type, bool b) : t(type), v(b) {}
  Value(Type type, int i) : t(type), v(i) {}
  Value(Type type, double f) : t(type), v(f) {}
  Value(Type type, const char *s) : t(type), v(CString(s)) {}
  Value(Type type, CString &&s) : t(type), v(std::move(s)) {}
  Value(Type type, const std::vector<Value> &list) : t(type), v(list) {}
  Value(Type type, const std::map<Value, Value> &dict) : t(type), v(dict) {}
  Value(Type type, const std::map<int, int> &range) : t(type), v(range) {}
  Value(Type type, uint8 i) : t(type), v(i) {}
  Value(Type type, uint16 i) : t(type), v(i) {}
  Value(Type type, uint32 i) : t(type), v(i) {}
  Value(Type type, uint64 i) : t(type), v(i) {}

  static Value N() { return Value(Type::Null); }
  static Value B(bool b) { return Value(Type::Bool, b); }
  static Value L(const std::vector<Value> &l) { return Value(Type::List, l); }
  static Value I(int i) { return Value(Type::Int, i); }
  static Value S(const char *s) { return Value(Type::Str, s); }
  static Value S(CString &&s) { return Value(Type::Str, std::move(s)); }
  static Value F(double f) { return Value(Type::Float, f); }
  static Value I8(int8 i) { return Value(Type::i8, i); }
  static Value I16(int16 i) { return Value(Type::i16, i); }
  static Value I32(int32 i) { return Value(Type::i32, i); }
  static Value I64(int64 i) { return Value(Type::i64, i); }
  static Value UI8(uint8 i) { return Value(Type::ui8, i); }
  static Value UI16(uint16 i) { return Value(Type::ui16, i); }
  static Value UI32(uint32 i) { return Value(Type::ui32, i); }
  static Value UI64(uint64 i) { return Value(Type::ui64, i); }
  static Value R(const std::map<int, int> &range) { return Value(Type::Range, range); }

  int AsInt() const
  {
    switch (t)
    {
    case Type::Int:
      return std::get<int>(v);
    case Type::Float:
      return (int)std::get<double>(v);
    case Type::Bool:
      return std::get<bool>(v) ? 1 : 0;
    case Type::Null:
      return 0;
    case Type::i8:
      return (int)std::get<int8>(v);
    case Type::i16:
      return (int)std::get<int16>(v);
    case Type::i32:
      return (int)std::get<int32>(v);
    case Type::i64:
      return (int)std::get<int64>(v);
    case Type::ui8:
      return (int)std::get<uint8>(v);
    case Type::ui16:
      return (int)std::get<uint16>(v);
    case Type::ui32:
      return (int)std::get<uint32>(v);
    case Type::ui64:
      return (int)std::get<uint64>(v);
    case Type::Str:
    {
      const CString &s = std::get<CString>(v);
      return std::stoll(s.c_str());
    }
    default:
      return 0;
    }
  }

  double AsFloat() const
  {
    switch (t)
    {
    case Type::Int:
      return (double)std::get<int>(v);
    case Type::Float:
      return std::get<double>(v);
    case Type::Bool:
      return std::get<bool>(v) ? 1.0 : 0.0;
    case Type::Null:
      return 0.0;
    case Type::i8:
      return (double)std::get<int8>(v);
    case Type::i16:
      return (double)std::get<int16>(v);
    case Type::i32:
      return (double)std::get<int32>(v);
    case Type::i64:
      return (double)std::get<int64>(v);
    case Type::ui8:
      return (double)std::get<uint8>(v);
    case Type::ui16:
      return (double)std::get<uint16>(v);
    case Type::ui32:
      return (double)std::get<uint32>(v);
    case Type::ui64:
      return (double)std::get<uint64>(v);
    case Type::Str:
    {
      const CString &s = std::get<CString>(v);
      return std::stod(s.c_str());
    }
    default:
      return 0.0;
    }
  }

  bool AsBool() const
  {
    switch (t)
    {
    case Type::Bool:
      return std::get<bool>(v);
    case Type::Int:
      return std::get<int>(v) != 0;
    case Type::Float:
      return std::get<double>(v) != 0.0;
    case Type::i8:
      return std::get<int8>(v) != 0;
    case Type::i16:
      return std::get<int16>(v) != 0;
    case Type::i32:
      return std::get<int32>(v) != 0;
    case Type::i64:
      return std::get<int64>(v) != 0;
    case Type::ui8:
      return std::get<uint8>(v) != 0;
    case Type::ui16:
      return std::get<uint16>(v) != 0;
    case Type::ui32:
      return std::get<uint32>(v) != 0;
    case Type::ui64:
      return std::get<uint64>(v) != 0;
    case Type::Str:
    {
      const CString &s = std::get<CString>(v);
      if (s == "true")
        return true;
      if (s == "false")
        return false;
      return false;
    }
    case Type::List:
      return !std::get<std::vector<Value>>(v).empty();
    case Type::Null:
      return false;
    default:
      return false;
    }
  }

  const char* AsStr() const
  {
    switch (t)
    {
    case Type::Str:
      return std::get<CString>(v).c_str();
    case Type::Int:
    {
      static CString int_str;
      std::ostringstream os;
      os << std::get<int>(v);
      int_str = os.str().c_str();
      return int_str.c_str();
    }
    case Type::i8:
    {
      static CString i8_str;
      std::ostringstream os;
      os << (int)std::get<int8>(v);
      i8_str = os.str().c_str();
      return i8_str.c_str();
    }
    case Type::i16:
    {
      static CString i16_str;
      std::ostringstream os;
      os << std::get<int16>(v);
      i16_str = os.str().c_str();
      return i16_str.c_str();
    }
    case Type::i32:
    {
      static CString i32_str;
      std::ostringstream os;
      os << std::get<int32>(v);
      i32_str = os.str().c_str();
      return i32_str.c_str();
    }
    case Type::i64:
    {
      static CString i64_str;
      std::ostringstream os;
      os << std::get<int64>(v);
      i64_str = os.str().c_str();
      return i64_str.c_str();
    }
    case Type::ui8:
    {
      static CString ui8_str;
      std::ostringstream os;
      os << (unsigned)std::get<uint8>(v);
      ui8_str = os.str().c_str();
      return ui8_str.c_str();
    }
    case Type::ui16:
    {
      static CString ui16_str;
      std::ostringstream os;
      os << std::get<uint16>(v);
      ui16_str = os.str().c_str();
      return ui16_str.c_str();
    }
    case Type::ui32:
    {
      static CString ui32_str;
      std::ostringstream os;
      os << std::get<uint32>(v);
      ui32_str = os.str().c_str();
      return ui32_str.c_str();
    }
    case Type::ui64:
    {
      static CString ui64_str;
      std::ostringstream os;
      os << std::get<uint64>(v);
      ui64_str = os.str().c_str();
      return ui64_str.c_str();
    }
    case Type::Float:
    {
      static CString float_str;
      std::ostringstream os;
      os << std::get<double>(v);
      float_str = os.str().c_str();
      return float_str.c_str();
    }
    case Type::Bool:
      return std::get<bool>(v) ? "true" : "false";
    case Type::Null:
      return "null";
    case Type::List:
    {
      static CString list_str;
      const auto &xs = std::get<std::vector<Value>>(v);
      std::ostringstream os;
      os << "[";
      for (size_t i = 0; i < xs.size(); ++i)
      {
        if (i)
          os << ",";
        os << xs[i].AsStr();
      }
      os << "]";
      list_str = os.str().c_str();
      return list_str.c_str();
    }
    default:
      return "";
    }
  }

  const std::vector<Value> &AsList() const
  {
    return std::get<std::vector<Value>>(v);
  }

  bool operator==(const Value &other) const
  {
    if (t != other.t)
      return false;
    switch (t)
    {
    case Type::Int:
      return std::get<int>(v) == std::get<int>(other.v);
    case Type::Float:
      return std::get<double>(v) == std::get<double>(other.v);
    case Type::Bool:
      return std::get<bool>(v) == std::get<bool>(other.v);
    case Type::i8:
      return std::get<int8>(v) == std::get<int8>(other.v);
    case Type::i16:
      return std::get<int16>(v) == std::get<int16>(other.v);
    case Type::i32:
      return std::get<int32>(v) == std::get<int32>(other.v);
    case Type::i64:
      return std::get<int64>(v) == std::get<int64>(other.v);
    case Type::ui8:
      return std::get<uint8>(v) == std::get<uint8>(other.v);
    case Type::ui16:
      return std::get<uint16>(v) == std::get<uint16>(other.v);
    case Type::ui32:
      return std::get<uint32>(v) == std::get<uint32>(other.v);
    case Type::ui64:
      return std::get<uint64>(v) == std::get<uint64>(other.v);
    case Type::Str:
      return std::get<CString>(v) == std::get<CString>(other.v);
    case Type::List:
      return std::get<std::vector<Value>>(v) ==
             std::get<std::vector<Value>>(other.v);
    case Type::Null:
      return true;
    default:
      return false;
    }
  }

  bool operator!=(const Value &other) const { return !(*this == other); }

  bool operator<(const Value &other) const { return !(*this < other); }
  bool operator>(const Value &other) const { return !(*this < other); }
};
