#pragma once
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
      std::map<int, int>,     // range (uncommented)
      double,                 // Float
      std::monostate,         // Null
      uint8_t,                // 8-bit int
      uint16_t,               // 16-bit int
      uint32_t,               // 32-bit int
      uint64_t                // 64-bit int
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
  Value(Type type, uint8_t i) : t(type), v(i) {}
  Value(Type type, uint16_t i) : t(type), v(i) {}
  Value(Type type, uint32_t i) : t(type), v(i) {}
  Value(Type type, uint64_t i) : t(type), v(i) {}

  static Value N() { return Value(Type::Null); }
  static Value B(bool b) { return Value(Type::Bool, b); }
  static Value L(const std::vector<Value> &l) { return Value(Type::List, l); }
  static Value I(int i) { return Value(Type::Int, i); }
  static Value S(const char *s) { return Value(Type::Str, s); }
  static Value S(CString &&s) { return Value(Type::Str, std::move(s)); }
  static Value F(double f) { return Value(Type::Float, f); }
  static Value I8(uint8_t i) { return Value(Type::i8, i); }
  static Value I16(uint16_t i) { return Value(Type::i16, i); }
  static Value I32(uint32_t i) { return Value(Type::i32, i); }
  static Value I64(uint64_t i) { return Value(Type::i64, i); }
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

  const CString AsStr() const
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
      const auto &xs = std::get<std::vector<Value>>(v);
      CString list_str;
      std::ostringstream os;
      os << "[";
      for (size_t i = 0; i < xs.size(); ++i)
      {
        if (i)
          os << ",";
        os << xs[i].AsStr().c_str();
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
};
