#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <conio.h> // Provides Windows _getch()
#include "../include/bytecode.hpp"
#include "../include/types.hpp"
#include "../include/value.hpp"
#include "../include/vm.hpp"
#include "../include/sso.hpp"
#include "../include/plugin.hpp"
#include "../include/macros.hpp"
#include "../include/io.hpp"

namespace minis {
  /*=============================
  =          Values/Env         =
  =============================*/

  // FIXME: Errors shouldn't be baked in unless debug code was added. The source shouldn't be assumed to exist.
  // FIXME: Should use view over raw pointer
  // inline const Source* src = nullptr;

  // Built-in function handler signature
  using BuiltinFn = std::function<Value(std::vector<Value>&)>;

  // Registry of built-in functions
  // FIXME: Instead of storing every function as a string, we should store it some other way, like an enum or computed goto, to improve speed.
  inline static std::unordered_map<CString, BuiltinFn> builtins = {
    // FIXME: We need to add the ability to add the end line manually.
    // Args can be Value or
    {"print", [](std::vector<Value>& args) {
      size_t arg_amnt = sizeof(args);
      for (size_t i = 0; i < arg_amnt; i++) {
        fputs(args[i].AsStr(), stdout);
        if (i < arg_amnt) {
          fputs(" ", stdout);
        }
      }
      return Value::N();
    }},
    {"abs", [](std::vector<Value>& args) {
      auto val = args[0];
      if (val.t == Type::Float) return Value::F(std::abs(val.AsFloat()));
      return Value::I(std::abs(val.AsInt()));
    }},
    {"neg", [](std::vector<Value>& args) {
      auto val = args[0];
      if (val.t == Type::Float) return Value::F(-val.AsFloat());
      return Value::I(-val.AsInt());
    }},

    // FIXME: [comment expansion]
    /* Range Becoming A Type
    *
    * Rather than range be a function, we can bake it into the opcodes and make it a type
    * This gives us greater control
    * We can store just the two values.
    * For something like:
    * int x = 34;
    * if (x in range(1,50)) {
    *   print("It's in!! :D");
    * }
    * It would come out like this:
    * int x is 34
    * if x is between one (1) and 50, print "It's in!! :D"
    * This would give us greater control, as our current version is held as a function baked into the VM.
    * This is also a HUGE load off of RAM, since we not store two integers, rather than every number, one (1) through 50.
    */
    // Memory-efficient range: stores only start and end values, not the entire list
    // Usage: range(5) -> Range(0,5) or range(2,5) -> Range(2,5)
    {"range", [](std::vector<Value>& args) {
      int start = 0, end;
      if (args.size() == 1) {
        end = args[0].AsInt();
      } else {
        start = args[0].AsInt();
        end = args[1].AsInt();
      }

      std::map<int, int> range_map;
      range_map[0] = start;  // Store start at key 0
      range_map[1] = end;    // Store end at key 1
      return Value::R(range_map);
    }},
    {"max", [](std::vector<Value>& args) {
      Value max = args[0];
      for (size_t i = 1; i < args.size(); i++) {
        if (args[i] > max) max = args[i];
      }
      return max;
    }},
    {"min", [](std::vector<Value>& args) {
      Value min = args[0];
      for (size_t i = 1; i < args.size(); i++) {
        if (args[i] < min) min = args[i];
      }
      return min;
    }},
    {"sort", [](std::vector<Value>& args) {
      std::vector<Value> list = args[0].AsList();
      std::sort(list.begin(), list.end(),
        [](const Value& a, const Value& b) { return a.AsFloat() < b.AsFloat(); });
      return Value::L(list);
    }},
    {"reverse", [](std::vector<Value>& args) -> Value {
      if (args[0].t == Type::List) {
        std::vector<Value> list = args[0].AsList();
        std::reverse(list.begin(), list.end());
        return Value::L(list);
      } else if (args[0].t == Type::Str) {
        const char* str = args[0].AsStr();
        std::string reversed(str);
        std::reverse(reversed.begin(), reversed.end());
        return Value::S(CString(reversed.c_str()));
      }
    }},
    {"sum", [](std::vector<Value>& args) {
      const auto& list = args[0].AsList();
      Value sum = Value::I(0);
      for (const auto& v : list) {
        if (v.t == Type::Float) sum = Value::F(sum.AsFloat() + v.AsFloat());
        else sum = Value::I(sum.AsInt() + v.AsInt());
      }
      return sum;
    }},
    {"input", [](std::vector<Value>& args) {
      if (!args.empty()) {
        // FIXME: Should use buffer.hpp instea of cout
        std::cout << args[0].AsStr();
      }
      char input[1024];
      if (fgets(input, sizeof(input), stdin)) {
        // Remove trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
        return Value::S(input);
      }
      return Value::S("");
    }},
    {"len", [](std::vector<Value>& args) -> Value {
      const auto& arg = args[0];
      if (arg.t == Type::List) {
        return Value::I(static_cast<long long>(std::get<std::vector<Value>>(arg.v).size()));
      } else if (arg.t == Type::Str) {
        return Value::I(static_cast<long long>(std::strlen(arg.AsStr())));
      }
      return Value::N();
    }},
    {"split", [](std::vector<Value>& args) -> Value {
      const char* str = args[0].AsStr();
      const char* delim = args[1].AsStr();

      std::vector<Value> result;
      std::string s(str);
      std::string delimiter(delim);

      size_t pos = 0;
      while ((pos = s.find(delimiter)) != std::string::npos) {
        result.push_back(Value::S(s.substr(0, pos).c_str()));
        s.erase(0, pos + delimiter.length());
      }
      result.push_back(Value::S(s.c_str()));

      return Value::L(result);
    }},
    {"upper", [](std::vector<Value>& args) -> Value {
      const char* str = args[0].AsStr();
      std::string result(str);
      std::transform(result.begin(), result.end(), result.begin(), ::toupper);
      return Value::S(result.c_str());
    }},
    {"lower", [](std::vector<Value>& args) -> Value {
      const char* str = args[0].AsStr();
      std::string result(str);
      std::transform(result.begin(), result.end(), result.begin(), ::tolower);
      return Value::S(result.c_str());
    }},
    {"round", [](std::vector<Value>& args) -> Value {
      return Value::I((long long)std::round(args[0].AsFloat()));
    }},
    {"random", [](std::vector<Value>& args) -> Value {
      if (args.empty()) {
        return Value::F((double)rand() / RAND_MAX);
      } else {
        long long max_val = args[0].AsInt();
        return Value::I(rand() % max_val);
      }
    }},

    {"open", [](std::vector<Value>& args) -> Value {
      const char* filename = args[0].AsStr();
      const char* mode_str = args[1].AsStr();
      // FIXME: This should be a switch instead
      // Translate Minis modes to C modes
      const char* c_mode = "r";
      // FIXME: [Comment expanded]
      // Possibly delegate to our VM instead of host?
      if (strcmp(mode_str, "r") == 0) c_mode = "r";
      else if (strcmp(mode_str, "rb") == 0) c_mode = "rb";
      else if (strcmp(mode_str, "rs") == 0) c_mode = "r";  // read specific = read but read only a section, rather than put the whole file into mem
      else if (strcmp(mode_str, "w") == 0) c_mode = "w";
      else if (strcmp(mode_str, "wb") == 0) c_mode = "wb";
      else if (strcmp(mode_str, "ws") == 0) c_mode = "w";  // write specific = write but write to a specific area rather than rewrite the whole thing
      /*write targeted = append but write anywhere, not just the end, shifting alldata after up
      * For example:
      * appending ", " to five (5) in "Helloworld" makes it "Hello, world"
      */
      else if (strcmp(mode_str, "wt") == 0) c_mode = "a";
      else if (strcmp(mode_str, "a") == 0) c_mode = "a";   // also support append directly

      FILE* file = fopen(filename, c_mode);
      if (!file) {
        return Value::I(-1); // Error code
      }

      // Store file pointer as integer (simple approach)
      return Value::I((long long)(uintptr_t)file);
    }},

    {"close", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)args[0].AsInt();
      if (file && file != stdin && file != stdout && file != stderr) {
        fclose(file);
      }
      return Value::I(0);
    }},

    {"write", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)args[0].AsInt();
      const char* data = args[1].AsStr();

      // We should use exit(1), not abort()
      if (!file) std::exit(1);

      size_t written = fwrite(data, 1, strlen(data), file);
      return Value::I((long long)written);
    }},

    {"read", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)args[0].AsInt();
      long long size = args[1].AsInt();

      if (!file) std::abort();
      if (size <= 0) return Value::S("");

      char* buffer = (char*)malloc(size + 1);
      size_t bytes_read = fread(buffer, 1, size, file);
      buffer[bytes_read] = '\0';

      Value result = Value::S(buffer);
      free(buffer);
      return result;
    }},

    {"flush", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)args[0].AsInt();
      if (file) {
        fflush(file);
      }
      return Value::I(0);
    }},
    // FIXME: We need better type checking
    {"typeof", [](std::vector<Value>& args) -> Value {
      switch (args[0].t) {
        case Type::Int:   return Value::I(0);
        case Type::Float: return Value::I(1);
        case Type::Str:   return Value::I(2);
        case Type::Bool:  return Value::I(3);
        case Type::List:  return Value::I(4);
        case Type::Null:  return Value::I(5);
        default: std::abort();
      }
    }},

    // FIXME: type checks should join into one
    {"isInt", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::Int);
    }},

    {"isFloat", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::Float);
    }},

    {"isString", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::Str);
    }},

    {"isList", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::List);
    }},

    {"isBool", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::Bool);
    }},

    {"isNull", [](std::vector<Value>& args) -> Value {
      return Value::B(args[0].t == Type::Null);
    }}
  };

  void Coerce(Type t, Value& v) {
    if (v.t == t) return;
    switch (t) {
      case Type::Int:   v = Value::I(v.AsInt()); break;
      case Type::Float: v = Value::F(v.AsFloat()); break;
      case Type::Bool:  v = Value::B(v.AsBool()); break;
      case Type::List:  v = Value::L(v.AsList()); break;
      case Type::Str:   v = Value::S(v.AsStr()); break;
      case Type::Null:  v = Value::N(); break;
      default: break;
    }
  }

  struct Env {
    struct Var {
      Type declared;
      Value val;
      Var(Type t, Value v) : declared(t), val(v) {}
    };

    std::unordered_map<CString, Var> m;
    Env* parent = nullptr;
    Value val;

    explicit Env(Env* p = nullptr) : parent(p), val{} {}

    bool ExistsLocal(const CString& n) const { return m.find(n) != m.end(); }

    // FIXME: This should be recursive, not limitied to two parent scopes
    bool Exists(const CString& n) const { return ExistsLocal(n) || (parent && parent->Exists(n)); }

    // FIXME: should not return dummy, we assume it will exist. Compiler handles those errors.
    const Var& Get(const CString& n) const {
      auto it = m.find(n);
      if (it != m.end()) return it->second;
      if (parent) return parent->Get(n);
      std::abort();
    }

    void Declare(const CString& n, Type t, Value v) {
      Coerce(t, v); m.emplace(n, Var{t, v});
    }

    void Set(const CString& n, Value v) {
      auto it = m.find(n);
      if (it != m.end()) {
        Coerce(it->second.declared, v);
        it->second.val = v;
        return;
      }
      if (parent) {
        parent->Set(n, v);
        return;
      }
      std::abort();
    }

    void SetOrDeclare(const CString& n, Value v) {
      if (ExistsLocal(n))
        Set(n, v);
      else if (parent && parent->Exists(n))
        parent->Set(n, v);
      else
        Declare(n, v.t, v);
    }

    bool Erase(const CString& n) { return m.erase(n) != 0; }

    bool Unset(const CString& n) {
      if (Erase(n)) return true;
      return parent ? parent->Unset(n) : false;
    }
  };

  struct VMEngine {
    Env globals;

    FILE* f = nullptr;
    uint64 ip = 0, table_off = 0, code_end = 0;
    std::vector<Value> stack;

    // FIXME: Strip data MVME doesn't need
    struct Frame {
      uint64 ret_ip;
      std::unique_ptr<Env> env;   // heap-allocated, stable address
      bool isVoid = false;
      bool typed  = false;
      Type ret    = Type::Int;

      Frame(uint64 rip, std::unique_ptr<Env> e, bool v, bool t, Type r)
        : ret_ip(rip), env(std::move(e)), isVoid(v), typed(t), ret(r) {}
    };

    std::vector<Frame> frames;

    // FIXME: We don't need to know void or typed at runtime
    struct FnMeta {
      uint64 entry;
      bool isVoid;
      bool typed;
      Type ret;
      std::vector<CString> params;
    };
    std::unordered_map<CString, FnMeta> fnEntry;

    explicit VMEngine() {}
    ~VMEngine() { if (f) fclose(f); }

    inline void jump(uint64 target) { ip = target; fseek(f, (long)ip, SEEK_SET); }

    // unsigned ints
    inline uint8 GETu8() { uint8 v; fread(&v, 1, 1, f); ip++; return v; }
    inline uint16 GETu16() { uint16 v; fread(&v, 2, 1, f); ip+=2; return v; }
    inline uint32 GETu32() { uint32 v; fread(&v, 4, 1, f); ip+=4; return v; }
    inline uint64 GETu64() { uint64 v; fread(&v, 8, 1, f); ip+=8; return v; }

    // signed ints
    inline int8 GETs8() { int8 v; fread(&v, 1, 1, f); ip++; return v; }
    inline int8 GETs16() { int16 v; fread(&v, 2, 1, f); ip+=2; return v; }
    inline int8 GETs32() { int32 v; fread(&v, 4, 1, f); ip+=4; return v; }
    inline int64 GETs64() { int64 v; fread(&v, 8, 1, f); ip += 8; return v; }

    inline double GETd64() { double v; fread(&v, 8, 1, f); ip += 8; return v; }
    inline CString GETstr() {
      uint64 n = GETu64();
      char* buffer = static_cast<char*>(malloc(n + 1));
      if (n) fread(buffer, 1, n, f);
      buffer[n] = '\0';
      CString result(buffer);
      free(buffer);
      ip += n;
      return result;
    }

    inline Value pop() {
      try {
        if (stack.empty()) {
          fprintf(stderr, "FATAL ERROR: Stack underflow. Tried to pop an empty stack.");
          std::exit(1);
        }
        // FIXME: This should use err.hpp (vm_err.hpp)
        if (stack.back().t == Type::Null) {
          fprintf(stderr, "FATAL ERROR: Stack had null top value.");
          std::exit(1);
        }
        Value v = std::move(stack.back());
        stack.pop_back();
        return v;
      } catch (const std::exception& e) {
        fprintf(stderr, "FATAL ERROR: Stack operation failed:", e.what());
        std::exit(1);
      }
      return Value::N();
    }

    inline void push(Value v) { stack.push_back(std::move(v)); }

    inline void discard() {
      if (stack.empty()) {
        fprintf(stderr, "stack underflow; tried to empty an already empty stack");
        std::exit(1);
      }
      stack.pop_back();
    }

    inline void load(const CString& path) {
      f = fopen(path.c_str(), "rb");
      if (!f) throw std::runtime_error("cannot open bytecode");

      char magic[9] = {0};
      fread(magic, 1, 8, f);
      if (std::strcmp(magic, "AVOCADO1") != 0)
        throw std::runtime_error("bad bytecode verification");

      table_off = GETu64();
      uint64 fnCount = GETu64();
      uint64 entry_main = GETu64();
      uint64 line_map_off = GETu64(); // Skip line map offset (not used)

      // Header is now exactly 40 bytes (8 magic + 32 data), no padding
      ip = entry_main;
      code_end = table_off;

      fseek(f, (long)table_off, SEEK_SET);
      for (uint64 i = 0; i < fnCount; i++) {
        CString name = GETstr();
        uint64 entry = GETu64();
        // FIXME: isVoid & typed aren't needed at runtime
        bool isVoid = GETu8() != 0;
        bool typed = GETu8() != 0;
        Type ret = (Type)GETu8();
        uint64 pcnt = GETu64();
        std::vector<CString> params;
        params.reserve((size_t)pcnt);
        for (uint64 j = 0; j < pcnt; ++j) params.push_back(read_str(f));
        fnEntry[name] = FnMeta{ entry, isVoid, typed, ret, params };
      }

      // Read and load plugins if line_map_off points to plugin table
      // For now, seek to line_map_off and check if there's a plugin table after
      if (line_map_off > 0) {
        fseek(f, (long)line_map_off, SEEK_SET);
        // Skip line map
        uint64 line_map_count = GETu64();
        for (uint64 i = 0; i < line_map_count; i++) {
          GETu64(); // offset
          GETu32(); // line
        }

        // Now read plugin table
        uint64 plugin_count = GETu64();
        for (uint64 i = 0; i < plugin_count; ++i) {
          CString module_name = GETstr();
          CString library_path = GETstr();

          // Load plugin
          // FIXME: We should use the custom Minis buffer instead
          if (!PluginManager::load_plugin(module_name.c_str(), library_path.c_str())) {
            std::cerr << "Warning: Failed to load plugin " << module_name.c_str() << std::endl;
          }
        }
      }

      jump(entry_main);
      frames.push_back(Frame{ (uint64)-1, nullptr, true, false, Type::Int });
      frames.back().env = std::make_unique<Env>(&globals);
    }

    inline void run() {
      for (;;) {
        if (ip >= code_end) return;
        unsigned char op = GETu8();
        // FIXME: This needs to possibly lead to another switch, or have switches in the switches.
        switch (op >> 5) {
          case static_cast<int>(Register::LOGIC): {
            switch ((op << 3) >> 3) {
              case static_cast<int>(Logic::ADD): {
                Value b = pop();
                Value a = pop();
                // FIXME: This needs to be tested for bugs heavily; When it was found for sorting,
                //        I found the first if as an else if.
                if (a.t == Type::List) {
                  if (b.t == Type::List) {
                    std::vector<Value> result;
                    // FIXME: Prefer specific type over auto
                    const auto& L = std::get<std::vector<Value>>(a.v);
                    const auto& R = std::get<std::vector<Value>>(b.v);
                    result.reserve(L.size() + R.size());
                    result.insert(result.end(), L.begin(), L.end());
                    result.insert(result.end(), R.begin(), R.end());
                    push(Value::L(std::move(result)));
                  } else {
                    std::vector<Value> result = std::get<std::vector<Value>>(a.v);
                    result.push_back(b);
                    push(Value::L(std::move(result)));
                  }
                } else if (a.t == Type::Str || b.t == Type::Str) {
                  CString result = CString(a.AsStr()) + b.AsStr();
                  push(Value::S(std::move(result)));
                } else if (a.t == Type::Float || b.t == Type::Float) {
                  push(Value::F(a.AsFloat() + b.AsFloat()));
                } else if (a.t == Type::Int || b.t == Type::Int) {
                  push(Value::I(a.AsInt() + b.AsInt()));
                }
              } break;
              case static_cast<int>(Logic::EQUAL): {
                Value b = pop(), a = pop();
                bool eq = (a.t == b.t) ? (a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (a.AsFloat() == b.AsFloat()) : false);
                push(Value::B(eq));
              } break;
              case static_cast<int>(Logic::SUBTRACT): {
                Value b = pop(), a = pop();
                if ((a.t == Type::Int || a.t == Type::Float) && (b.t == Type::Int || b.t == Type::Float)) {
                  if (a.t == Type::Float || b.t == Type::Float) {
                    push(Value::F(a.AsFloat() - b.AsFloat()));
                  } else {
                    push(Value::I(a.AsInt() - b.AsInt()));
                  }
                }
              } break;
              case static_cast<int>(Logic::MULTIPLY): {
                Value b = pop(), a = pop();
                if (a.t == Type::Float || b.t == Type::Float)
                  push(Value::F(a.AsFloat() * b.AsFloat()));
                else
                  push(Value::I(a.AsInt() * b.AsInt()));
              } break;
              case static_cast<int>(Logic::DIVIDE): {
                Value b = pop(), a = pop();
                push(Value::F(a.AsFloat() / b.AsFloat()));
              } break;
              case static_cast<int>(Logic::JUMP_IF_FALSE):  { uint64 tgt = GETu64(); Value v = pop(); if (!v.AsBool()) jump(tgt); } break; // Jump if false
            }
          } break;
          case static_cast<int>(Register::VARIABLE): {
            switch (op & 0x1F) {
              case static_cast<int>(Variable::PUSH): {
                switch (op & 0xF0) {
                  case 0: {
                  unsigned char meta = GETu8();
                  uint8 type = meta >> 4;
                  // uint8_t signedness = meta & 0b11110111; // Not currently implemented; Commented to reduce unused variable warning
                  // NOTE: In this implementation, 0 is false, 1 is true
                  switch(type) {
                    case 0x00: push(Value::I8(GETs8())); break;
                    case 0x01: push(Value::I16(GETs16())); break;
                    case 0x02: push(Value::I32(GETs32())); break;
                    case 0x03: push(Value::I64(GETs64())); break;

                    case 0x04: push(Value::UI8(GETu8())); break;
                    case 0x05: push(Value::UI16(GETu16())); break;
                    case 0x06: push(Value::UI32(GETu32())); break;
                    case 0x07: push(Value::UI64(GETu64())); break;
                    case 0x08: push(Value::F(GETd64())); break;
                    case 0x09: {
                      unsigned char bool_val = meta;
                      bool_val & 0b00000100;
                      if (!(meta && 0b00000000)) {
                        push(Value::B(false));
                      } else {
                        push(Value::B(true));
                      }
                    } break;
                    default: {
                      fprintf(stderr, "FATAL ERROR: Unknown meta tag");
                      std::abort();
                    }
                  }
                  /*type = meta;
                  type |= 0b00001000;
                  switch(type) {
                    case 0x00: // Signed
                    case 0x01: // Unsigned
                  }*/
                } break;

                case 3: push(Value::S(GETstr())); break;
                case 4: {
                  uint64 n = GETu64();
                  std::vector<Value> xs; xs.resize(n);
                  for (uint64 i = 0; i < n; ++i) xs[n-1-i] = pop();
                  push(Value::L(std::move(xs)));
                } break;
                default: {
                  fprintf(stderr, "unknown literal type tag");
                  std::abort();
                }
              }
            } break;
            case static_cast<int>(Variable::SET): frames.back().env->SetOrDeclare(GETstr(), pop()); break;
            case static_cast<int>(Variable::DECLARE): {
              CString id = GETstr();
              // FIXME:
              uint64 tt = GETu64();
              Value v  = pop();
              if (tt == 0xECull) frames.back().env->Declare(id, v.t, v);
              else               frames.back().env->Declare(id, (Type)tt, v);
            } break;
            case static_cast<int>(Variable::GET): {
              CString id = GETstr();
              push(frames.back().env->Get(id).val);
            } break;

            // FIXME: This is possibly incomplete
            case static_cast<int>(Variable::UNSET): {
              CString id = GETstr();
              if (!frames.back().env->Unset(id)) {}
            } break;
          } break;


          case static_cast<int>(Register::GENERAL): {
            switch (op & 0x1F) {
              case static_cast<int>(General::HALT): return;
              case static_cast<int>(General::NOP): break;
              case static_cast<int>(General::POP): discard(); break;
            }
          } break;
          case static_cast<int>(Register::FUNCTION): {
            switch (op & 0x1F) {
              // FIXME: Could be simpler to implement via other MVME opcodes
              case static_cast<int>(Func::TAIL): {
              CString name = GETstr();
              uint64 argc = GETu64();
              std::vector<Value> args(argc);
              for (size_t i = 0; i < argc; ++i) args[argc-1-i] = pop();

              auto it = fnEntry.find(name);
              if (it == fnEntry.end()) {
                auto bit = builtins.find(name);
                if (bit == builtins.end()) {
                  // FIXME: Should have an error message
                  // FIXMENOTE: We can remove this, if it's completely preventable by the compiler :)
                  std::exit(1);
                }
                auto rv = bit->second(args);
                push(std::move(rv));
                break;
              }
              const auto& meta = it->second;

              // FIXME: We don't need to know isVoid or typed at runtime
              Frame& currentFrame = frames.back();
              currentFrame.isVoid = meta.isVoid;
              currentFrame.typed = meta.typed;
              currentFrame.ret = meta.ret;

              Env* callerEnv = frames[frames.size()-2].env.get();
              currentFrame.env = std::make_unique<Env>(callerEnv);

              for (size_t i = 0; i < meta.params.size() && i < args.size(); ++i) {
                // FIXME: We might be able to use just args[i] then do .t in the function
                currentFrame.env->Declare(meta.params[i], args[i].t, args[i]);
              }

              jump(meta.entry);
              } break;
            }
          } break;
          case static_cast<int>(Register::IMPORT): {

          } break;

          case NE: {
            Value b = pop(), a = pop();
            bool ne = (a.t == b.t) ? !(a == b)
                      : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                          ? (a.AsFloat() != b.AsFloat()) : true);
            push(Value::B(ne));
          } break;

          case LT: {
            Value b = pop(), a = pop();
            if (a.t == Type::Str && b.t == Type::Str)
              push(Value::B(std::strcmp(a.AsStr(), b.AsStr()) < 0));
            else
              push(Value::B(a.AsFloat() < b.AsFloat()));
          } break;

          case LE: {
            Value b = pop(), a = pop();
            if (a.t == Type::Str && b.t == Type::Str)
              push(Value::B(std::strcmp(a.AsStr(), b.AsStr()) <= 0));
            else
              push(Value::B(a.AsFloat() <= b.AsFloat()));
          } break;

          case AND: { Value b = pop(), a = pop(); push(Value::B(a.AsBool() && b.AsBool())); } break;
          case OR:  { Value b = pop(), a = pop(); push(Value::B(a.AsBool() || b.AsBool())); } break;

          case JMP: { uint64 tgt = GETu64(); jump(tgt); } break;

          case YIELD: {
            #ifdef _WIN32
              _getch();
            #else
              system("read -n 1");
            #endif
          } break;

          case CALL: {
            CString name = GETstr();
            uint64 argc = GETu64();
            std::vector<Value> args(argc);
            for (size_t i = 0; i < argc; ++i) args[argc-1-i] = pop();

            auto it = fnEntry.find(name);
            if (it == fnEntry.end()) {
              auto bit = builtins.find(name);
              if (bit == builtins.end()) {
                // Check plugin functions
                auto pfn = PluginManager::get_function(name.c_str());
                if (pfn) {
                  auto rv = pfn(args);
                  push(std::move(rv));
                  break;
                }
              }
              auto rv = bit->second(args);
              push(std::move(rv));
              break;
            }
            const auto& meta = it->second;

            frames.push_back(Frame{ ip, nullptr, meta.isVoid, meta.typed, meta.ret });
            Env* callerEnv = frames[frames.size()-2].env.get();
            frames.back().env = std::make_unique<Env>(callerEnv);

            for (size_t i = 0; i < meta.params.size() && i < args.size(); ++i) {
              frames.back().env->Declare(meta.params[i], args[i].t, args[i]);
            }

            jump(meta.entry);
          } break;

          case RET: {
            Value rv = pop();
            if (frames.size() == 1) return;
            if (frames.back().typed) { Coerce(frames.back().ret, rv); }
            uint64 ret = frames.back().ret_ip;
            frames.pop_back();
            jump(ret);
            push(rv);
          } break;

          case INDEX: {
            Value idxV = pop();
            Value base = pop();
            long long i = idxV.AsInt();
            if (base.t == Type::List) {
              // FIXME: Prefer explicit over auto
              auto& xs = std::get<std::vector<Value>>(base.v);
              push(xs[(size_t)i]);
            } else if (base.t == Type::Str) {
              const char* s = base.AsStr();
              // FIXME: This error is hard to prevent with just the compiler
              //        The error could go over expected amount, during run...
              /*size_t len = std::strlen(s);
              if (i < 0 || (size_t)i >= len) {
                Loc L = locate(p.i);
                ERR(L, "index out of bounds");
              }*/
              char single[2] = {s[i], '\0'};
              push(Value::S(single));
            }
          } break;

          case RET_VOID: {
            if (frames.size() == 1) return;
            uint64 ret = frames.back().ret_ip;
            frames.pop_back();
            jump(ret);
            push(Value::I(0));
          } break;

          default: {
            std::cerr<< "FATAL ERROR: Bad or unknown opcode.";
          }
        }
      }
    }
  };

  VM::VM() = default;
  VM::~VM() = default;

  void VM::load(const CString& path) {
    engine = std::make_unique<VMEngine>();
    engine->load(path);
  }

  void VM::run() {
    if (engine) {
      engine->run();
    }
  }

  // Global run function
  void run(const CString& path) {
    VMEngine vm;
    vm.load(path);
    vm.run();
  }
}
