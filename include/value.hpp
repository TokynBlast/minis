#pragma once
#include <variant>
#include <vector>
#include <sstream>
#include "err.hpp"
#include "types.hpp"
#include "pos.hpp"
#include "sso.hpp"

using namespace lang;

struct Value {
  Pos P;

  Value() = default;

  // Accept a Source instead of a temporary string to keep a stable pointer.
  explicit Value(const Source& source) {
    P.src = source.name.c_str();  // Direct assignment to CString
    P.line = 1;
    P.col = 1;
  }

  Type t = Type::Null;
  std::variant<
    std::monostate,          // Null
    bool,                    // Bool
    std::vector<Value>,      // List
    long long,               // Int
    CString,                 // Str
    double                   // Float
  > v;

  Value(Type type) : t(type) {}
  Value(Type type, bool b) : t(type), v(b) {}
  Value(Type type, long long i) : t(type), v(i) {}
  Value(Type type, double f) : t(type), v(f) {}
  Value(Type type, const char* s) : t(type), v(CString(s)) {}
  Value(Type type, CString&& s) : t(type), v(std::move(s)) {}
  Value(Type type, const std::vector<Value>& list) : t(type), v(list) {}
  Value(Type type, const Pos& pos) : t(type) { P = pos; }

  static Value N() { return Value(Type::Null); }
  static Value B(bool b) { return Value(Type::Bool, b); }
  static Value L(const std::vector<Value>& l) { return Value(Type::List, l); }
  static Value I(long long i) { return Value(Type::Int, i); }
  static Value S(const char* s) { return Value(Type::Str, s); }
  static Value S(CString&& s) { return Value(Type::Str, std::move(s)); }
  static Value F(double f) { return Value(Type::Float, f); }

private:
  Loc locate(std::size_t extra = 0) const {
    Loc loc;
    loc.line = static_cast<int>(P.line);
    loc.col = static_cast<int>(P.col + extra);
    loc.src = P.src;
    return loc;
  }

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
          // Build error message using ostringstream
          std::ostringstream msg;
          msg << "cannot convert string '" << s.c_str() << "' to int (must be a valid number)";
          ERR(locate((size_t)loc), msg.str());
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
          ERR(locate((size_t)loc), msg.str());
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
        ERR(locate((size_t)loc), msg.str());
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