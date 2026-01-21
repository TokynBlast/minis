#pragma once
#include <variant>
#include <vector>
#include <map>
#include <sstream>
#include <string>
#include <stdint.h>

#include "types.hpp"
#include "macros.h"
#include "../fast_io/include/fast_io.h"


using namespace minis;

enum class TriBool : uint8 {
  True = 0,
  False = 1,
  Unknown = 2
};

// Helper structs to avoid type ambiguity in std::variant
struct Value
{
  Value() = default;
  Value(const Value&) = default;
  Value& operator=(const Value&) = default;

  Type t = Type::Null;
  std::variant<
      bool,                     // Bool
      std::string,              // String
      std::vector<Value>,       // List
      std::map<Value, Value>,   // dictionary
      std::map<uint64, uint64>, // range // Should add variant for 8-125, signed and unsigned :)
      double,                   // float
      std::monostate,           // null
      int8,                     // 8-bit int
      int16,                    // 16-bit int
      int32,                    // 32-bit int (also covers 'int' on most platforms)
      int64,                    // 64-bit int
      int128,                   // 128-bit int
      uint8,                    // unsigned 8-bit int
      uint16,                   // unsigned 16-bit int
      uint32,                   // unsigned 32-bit
      uint64,                   // unsigned 64-bit int
      uint128,                  // unsigned
      TriBool                   // TriBool
      >
      v;

  Value(Type type) : t(type) {}
  Value(Type type, bool b) : t(type), v(b) {}
  Value(Type type, double f) : t(type), v(f) {}
  Value(Type type, std::string &&s) : t(type), v(std::move(s)) {}
  Value(Type type, const std::vector<Value> &list) : t(type), v(list) {}
  Value(Type type, const std::map<Value, Value> &dict) : t(type), v(dict) {}
  Value(Type type, const std::map<uint64, uint64> &range) : t(type), v(range) {}
  Value(Type type, int8 i8) : t(type), v(i8) {}
  Value(Type type, int16 i16) : t(type), v(i16) {}
  Value(Type type, int32 i32) : t(type), v(i32) {}
  Value(Type type, int64 i64) : t(type), v(i64) {}
  Value(Type type, uint8 ui8) : t(type), v(ui8) {}
  Value(Type type, uint16 ui16) : t(type), v(ui16) {}
  Value(Type type, uint32 ui32) : t(type), v(ui32) {}
  Value(Type type, uint64 ui64) : t(type), v(ui64) {}
  Value(Type type, TriBool tb) : t(type), v(tb) {}


  static Value Null() { return Value(Type::Null); }
  static Value Bool(bool b) { return Value(Type::Bool, b); }
  static Value List(const std::vector<Value> &l) { return Value(Type::List, l); }
  static Value Str(std::string &&s) { return Value(Type::Str, std::move(s)); }
  static Value Float(double f) { return Value(Type::Float, f); }
  static Value I8(int8 i) { Value v(Type::i8); v.v = static_cast<int8>(i); return v; }
  static Value I16(int16 i) { Value v(Type::i16); v.v = static_cast<int16>(i); return v; }
  static Value I32(int32 i) { Value v(Type::i32); v.v = static_cast<int32>(i); return v; }
  static Value I64(int64 i) { Value v(Type::i64); v.v = static_cast<int64>(i); return v; }
  static Value UI8(uint8 i) { Value v(Type::ui8); v.v = static_cast<uint8>(i); return v; }
  static Value UI16(uint16 i) { Value v(Type::ui16); v.v = static_cast<uint16>(i); return v; }
  static Value UI32(uint32 i) { Value v(Type::ui32); v.v = static_cast<uint32>(i); return v; }
  static Value UI64(uint64 i) { Value v(Type::ui64); v.v = static_cast<uint64>(i); return v; }
  static Value Range(const std::map<uint64, uint64> &range) { Value v(Type::Range); v.v = range; return v; }
  static Value Void() {return Value(Type::Void); }
  static Value TriBool(TriBool tb) { return Value(Type::TriBool, tb);  }
  // FIXME: Need a standard dict input style
  // std::unordered_map<Value::v, Value::v> may work
  // static Value Dict()

  bool operator==(const Value &other) const
  {
    if (t != other.t)
      return false;
    switch (t)
    {
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
      return std::get<std::string>(v) == std::get<std::string>(other.v);
    case Type::List:
      return std::get<std::vector<Value>>(v) == std::get<std::vector<Value>>(other.v);
    case Type::Null:
      return true;
    default:
      return false;
    }
  }

  bool operator!=(const Value &other) const { return !(*this == other); }

  bool operator<(const Value &other) const { return !(*this < other); }
  bool operator>(const Value &other) const { return !(*this > other); }
};
using fast_io::io::print;

inline void print_value(const Value& v) {
  switch (v.t) {
    case Type::Null: {
      ("FATAL ERROR: Cannot print null type");
      exit(1);
    }
    case Type::Bool:
      print(std::string(std::get<bool>(v.v) ? "true" : "false"));
      break;
    case Type::TriBool: {
      TriBool tb = std::get<TriBool>(v.v);
      switch (tb) {
        case TriBool::True: print("true"); break;
        case TriBool::False: print("false"); break;
        case TriBool::Unknown: print("unknown"); break;
      }
      break;
    }
    case Type::Str:
      print('"', std::get<std::string>(v.v), '"');
      break;
    case Type::Float:
      print(std::get<double>(v.v));
      break;
    case Type::i8:
      print(std::get<int8>(v.v));
      break;
    case Type::i16:
      print(std::get<int16>(v.v));
      break;
    case Type::i32:
      print(std::get<int32>(v.v));
      break;
    case Type::i64:
      print(std::get<int64>(v.v));
      break;
    case Type::ui8:
      print(std::get<uint8>(v.v));
      break;
    case Type::ui16:
      print(std::get<uint16>(v.v));
      break;
    case Type::ui32:
      print(std::get<uint32>(v.v));
      break;
    case Type::ui64:
      print(std::get<uint64>(v.v));
      break;
    case Type::List: {
      print("[");
      const auto& list = std::get<std::vector<Value>>(v.v);
      for (size_t i = 0; i < list.size(); ++i) {
        print_value(list[i]);
        if (i + 1 < list.size()) print(" ");
      }
      print("]");
      break;
    }
    default:
      print("<unknown>");
  }
}
