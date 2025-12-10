#pragma once
#include <variant>
#include <vector>
#include <sstream>
#include <stdint.h>
#include "types.hpp"
#include "sso.hpp"

using namespace lang;

struct Value {
  Pos P;

  Value() = default;

  // Accept a Source instead of a temporary string to keep a stable pointer.
  explicit Value(const Source& source) {
    P.src = source.name.c_str();  // Direct assignment to CString
  }

  Type t = Type::Null;
  std::variant<
    CString,             // Str
    int,                 // Int
    bool,                // Bool
    std::vector<Value>,  // List
    double,              // Float
    std::monostate,      // Null
    int8_t,              // 8-bit int
    int16_t,             // 16-bit int
    int32_t,             // 32-bit int
    int64_t,             // 64-bit int
    __int128,            // GNU 128-bit int
    uint8_t,             // unsigned 8-bit int
    uint16_t,            // unsigned 16-bit int
    uint32_t,            // unsigned 32-bit int
    uint64_t,            // unsigned　64-bit int
    unsigned __int128    // GNU unsigned 128-bit int
  > v;

  Value(Type type) : t(type) {}
  Value(Type type, bool b) : t(type), v(b) {}
  Value(Type type, int i) : t(type), v(i) {}
  Value(Type type, double f) : t(type), v(f) {}
  Value(Type type, const char* s) : t(type), v(CString(s)) {}
  Value(Type type, CString&& s) : t(type), v(std::move(s)) {}
  Value(Type type, const std::vector<Value>& list) : t(type), v(list) {}
  Value(Type type, const int8_t i) : t(type), v(i) {}
  Value(Type type, const int16_t i) : t(type), v(i) {}
  Value(Type type, const int32_t i) : t(type), v(i) {}
  Value(Type type, const int64_t i) : t(type), v(i) {}
  Value(Type type, const __int128 i) : t(type), v(i) {}
  Value(Type type, const uint8_t ui) : t(type), v(ui) {}
  Value(Type type, const uint16_t ui) : t(type), v(ui) {}
  Value(Type type, const uint32_t ui) : t(type), v(ui) {}
  Value(Type type, const uint64_t ui) : t(type), v(ui) {}
  Value(Type type, const unsigned __int128 ui) : t(type), v(ui) {}

  static Value N() { return Value(Type::Null); }
  static Value B(bool b) { return Value(Type::Bool, b); }
  static Value L(const std::vector<Value>& l) { return Value(Type::List, l); }
  static Value I(int i) { return Value(Type::Int, i); }
  // FIXME: String should not copy, too much possible overhead
  static Value S(const char* s) { return Value(Type::Str, s); }
  static Value S(CString&& s) { return Value(Type::Str, std::move(s)); }
  static Value F(double f) { return Value(Type::Float, f); }
  static Value I8(int8_t i) {return Value(Type::i8, i); }
  static Value I16(int16_t i) {return Value(Type::i16, i); }
  static Value I32(int32_t i) {return Value(Type::i32, i); }
  static Value I64(int64_t i) {return Value(Type::i64, i); }
  static Value I128(__int128 i) {return Value(Type::i128, i); }
  static Value UI8(uint8_t ui) {return Value(Type::ui8, ui); }
  static Value UI16(uint16_t ui) {return Value(Type::ui16, ui); }
  static Value UI32(uint32_t ui) {return Value(Type::ui32, ui); }
  static Value UI64(uint64_t ui) {return Value(Type::ui64, ui); }
  static Value UI128(unsigned __int128 i) {return Value(Type::ui128, ui); }

public:
  long long AsInt(auto loc) const {
    switch (t) {
      case Type::Int:   return std::get<long long>(v);
      case Type::Float: return (long long)std::get<double>(v);
      case Type::Bool:  return std::get<bool>(v) ? 1 : 0;
      case Type::Null:  return 0;
      case Type::Str: {
        const CString& s = std::get<CString>(v);
        try {
          return std::stoll(s.c_str());
        } catch (...) {
          std::ostringstream msg;
          msg << "cannot convert string '" << s.c_str() << "' to int (must be a valid number)";
          std::string msg_str = msg.str();
          ERR(locate((size_t)loc), msg_str.c_str());
          return 0;
        }
      }
      case Type::List:
        ERR(locate((size_t)loc), "cannot convert list to int");
        return 0;
      default:
        ERR(locate((size_t)loc), "unexpected error");
        return 0;
    }
  }

  double AsFloat(auto loc) const {
    switch (t) {
      case Type::Int:   return (double)std::get<long long>(v);
      case Type::Float: return std::get<double>(v);
      case Type::Bool:  return std::get<bool>(v) ? 1.0 : 0.0;
      case Type::Null:  return 0.0;
      case Type::List:
        ERR(locate((size_t)loc), "cannot convert list to float");
        return 0.0;
      case Type::Str: {
        const CString& s = std::get<CString>(v);
        try {
          return std::stod(s.c_str());
        } catch (...) {
          std::ostringstream msg;
          msg << "cannot convert string '" << s.c_str() << "' to float";
          std::string msg_str = msg.str();
          ERR(locate((size_t)loc), msg_str.c_str());  // Fix: Use .c_str()
          return 0.0;
        }
      }
      default:
        ERR(locate((size_t)loc), "unexpected error");
        return 0.0;
    }
  }

  bool AsBool(auto loc) const {
    switch (t) {
      case Type::Bool:  return std::get<bool>(v);
      case Type::Int:   return std::get<long long>(v) != 0;
      case Type::Float: return std::get<double>(v) != 0.0;
      case Type::Str: {
        const CString& s = std::get<CString>(v);
        if (s == "true") return true;
        if (s == "false") return false;
        std::ostringstream msg;
        msg << "cannot convert string '" << s.c_str() << "' to bool";
        std::string msg_str = msg.str();
        ERR(locate((size_t)loc), msg_str.c_str());
        return false;
      }
      case Type::List:  return !std::get<std::vector<Value>>(v).empty();
      case Type::Null:  return false;
      default:
        ERR(locate((size_t)loc), "unexpected error");
        return false;
    }
  }

  const char* AsStr() const {
    switch (t) {
      case Type::Str:   return std::get<CString>(v).c_str();
      case Type::Int: {
        static thread_local CString int_str;
        std::ostringstream os;
        os << std::get<long long>(v);
        int_str = os.str().c_str();
        return int_str.c_str();
      }
      case Type::Float: {
        static thread_local CString float_str;
        std::ostringstream os;
        os << std::get<double>(v);
        float_str = os.str().c_str();
        return float_str.c_str();
      }
      case Type::Bool:  return std::get<bool>(v) ? "true" : "false";
      case Type::Null:  return "null";
      case Type::List: {
        static thread_local CString list_str;
        const auto& xs = std::get<std::vector<Value>>(v);
        std::ostringstream os;
        os << "[";
        for (size_t i = 0; i < xs.size(); ++i) {
          if (i) os << ",";
          os << xs[i].AsStr();
        }
        os << "]";
        list_str = os.str().c_str();
        return list_str.c_str();
      }
    }
    return "";
  }

  const std::vector<Value>& AsList() const {
    return std::get<std::vector<Value>>(v);
  }

  bool operator==(const Value& other) const {
    if (t != other.t) return false;
    switch (t) {
      case Type::Int:
        return std::get<long long>(v) == std::get<long long>(other.v);
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

  bool operator!=(const Value& other) const { return !(*this == other); }
};
