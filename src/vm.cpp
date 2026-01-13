#include <memory>
#include <unordered_map>
#include <vector>
// #include <iostream>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <array>
// #include <print>
#include <conio.h> // Provides Windows _getch()
#include "../include/bytecode.hpp"
#include "../include/types.hpp"
#include "../include/value.hpp"
#include "../include/vm.hpp"
#include "../include/plugin.hpp"
#include "../include/macros.hpp"
#include "../include/io.hpp"
// #include <fast_io.h>
#include "../fast_io/include/fast_io.h"

// using std::print;

// fast_io is here to replace standard iostream
// until one of the following occurs:
// iostream gets replaced or improved
// fast_io is depreceated
// something better is found

namespace minis {
  /*=============================
  =          Values/Env         =
  =============================*/
  using fast_io::io::print;
  using fast_io::io::scan;

  // Built-in function handler signature
  using BuiltinFn = std::function<Value(std::vector<Value>&)>;

  // Registry of built-in functions
  // FIXME: Instead of storing every function as a string, we should store it some other way, like an enum or computed goto, to improve speed.
  inline static std::unordered_map<std::string, BuiltinFn> builtins = {
    // FIXME: We need to add the ability to add the end line manually.
    // FIXME: We need to make print more customizable
    {"print", [](std::vector<Value>& args) {
      size_t arg_amnt = sizeof(args);
      for (size_t i = 0; i < arg_amnt; i++) {
        Value& val = args[i];
        switch (val.t) {
          case Type::Int:    print(std::get<int32>(val.v)); break;
          case Type::Float:  print(std::get<double>(val.v)); break;
          case Type::Str:    print(std::get<std::string>(val.v)); break;
          case Type::Bool:   print(std::get<bool>(val.v)); break;
          case Type::Null:   print(""); break;
          case Type::i8:     print(std::get<int8>(val.v)); break;
          case Type::i16:    print(std::get<int16>(val.v)); break;
          case Type::i32:    print(std::get<int32>(val.v)); break;
          case Type::i64:    print(std::get<int64>(val.v)); break;
          case Type::ui8:    print(std::get<uint8>(val.v)); break;
          case Type::ui16:   print(std::get<uint16>(val.v)); break;
          case Type::ui32:   print(std::get<uint32>(val.v)); break;
          case Type::ui64:   print(std::get<uint64>(val.v)); break;
          default:{
            print("FARAL EROR: Unknown type");
            exit(1);
          }
        }
        if (i < arg_amnt) {
          print(" ");
        }
      }
      // FIXME: print should return nothing
      return Value::Null();
    }},
    {"abs", [](std::vector<Value>& args) {
      auto val = args[0];
      if (val.t == Type::Float) return Value::Float(std::abs(std::get<double>(val.v)));
      return Value::Int(std::abs(std::get<int32>(val.v)));
    }},
    {"neg", [](std::vector<Value>& args) {
      auto& arg = args[0];
      if (arg.t == Type::Float) return Value::Float(-std::get<double>(arg.v));
      return Value::Int(-std::get<int32>(arg.v));
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
        end = std::get<int32>(args[0].v);
      } else {
        start = std::get<int32>(args[0].v);
        end = std::get<int32>(args[1].v);
      }

      std::map<int, int> range_map;
      range_map[0] = start;  // Store start at key 0
      range_map[1] = end;    // Store end at key 1
      return Value::Range(range_map);
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
      std::vector<Value> list = std::get<std::vector<Value>>(args[0].v);
      std::sort(list.begin(), list.end(),
        [](const Value& a, const Value& b) { return std::get<double>(a.v) < std::get<double>(b.v); });
      return Value::List(list);
    }},
    {"reverse", [](std::vector<Value>& args) -> Value {
      if (args[0].t == Type::List) {
        std::vector<Value> list = std::get<std::vector<Value>>(args[0].v);
        std::reverse(list.begin(), list.end());
        return Value::List(list);
      } else if (args[0].t == Type::Str) {
        std::string reversed = std::get<std::string>(args[0].v);
        std::reverse(reversed.begin(), reversed.end());
        return Value::Str(std::move(reversed));
      }
      exit(1);
    }},
    {"sum", [](std::vector<Value>& args) {
      const auto& list = std::get<std::vector<Value>>(args[0].v);
      Value sum = Value::Int(0);
      for (const auto& v : list) {
        if (v.t == Type::Float) sum = Value::Float(std::get<double>(sum.v) + std::get<double>(v.v));
        else sum = Value::Int(std::get<int32>(sum.v) + std::get<int32>(v.v));
      }
      return sum;
    }},
    // Print like print, using all values
    {"input", [](std::vector<Value>& args) {
      std::string input;
      if (!args.empty()) {
        print(std::get<std::string>(args[0].v));
        scan(input);
        return Value::Str(std::move(input));
      }
      return Value::Str("");
    }},
    {"len", [](std::vector<Value>& args) -> Value {
      const auto& arg = args[0];
      if (arg.t == Type::List) {
        return Value::Int(static_cast<long long>(std::get<std::vector<Value>>(arg.v).size()));
      } else if (arg.t == Type::Str) {
        return Value::Int(static_cast<long long>(std::get<std::string>(arg.v).length()));
      }
      return Value::Null();
    }},
    {"split", [](std::vector<Value>& args) -> Value {
      const std::string& str = std::get<std::string>(args[0].v);
      const std::string& delim = std::get<std::string>(args[1].v);

      std::vector<Value> result;
      std::string s(str);
      std::string delimiter(delim);

      size_t pos = 0;
      while ((pos = s.find(delimiter)) != std::string::npos) {
        result.push_back(Value::Str(s.substr(0, pos).c_str()));
        s.erase(0, pos + delimiter.length());
      }
      result.push_back(Value::Str(s.c_str()));

      return Value::List(result);
    }},
    {"upper", [](std::vector<Value>& args) -> Value {
      std::string result = std::get<std::string>(args[0].v);
      std::transform(result.begin(), result.end(), result.begin(), ::toupper);
      return Value::Str(std::move(result));
    }},
    {"lower", [](std::vector<Value>& args) -> Value {
      std::string result = std::get<std::string>(args[0].v);
      std::transform(result.begin(), result.end(), result.begin(), ::tolower);
      return Value::Str(std::move(result));
    }},
    {"round", [](std::vector<Value>& args) -> Value {
      return Value::Int((long long)std::round(std::get<double>(args[0].v)));
    }},
    {"random", [](std::vector<Value>& args) -> Value {
      if (args.empty()) {
        return Value::Float((double)rand() / RAND_MAX);
      } else {
        long long max_val = std::get<int32>(args[0].v);
        return Value::Int(rand() % max_val);
      }
    }},

    {"open", [](std::vector<Value>& args) -> Value {
      const char* filename = std::get<std::string>(args[0].v).c_str();
      const char* mode_str = std::get<std::string>(args[1].v).c_str();
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
        return Value::Int(-1); // Error code
      }

      // Store file pointer as integer (simple approach)
      return Value::Int((long long)(uintptr_t)file);
    }},

    {"close", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)std::get<int32>(args[0].v);
      if (file && file != stdin && file != stdout && file != stderr) {
        fclose(file);
      }
      return Value::Int(0);
    }},

    {"write", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)std::get<int32>(args[0].v);
      const char* data = std::get<std::string>(args[1].v).c_str();

      if (!file) std::exit(1);

      size_t written = fwrite(data, 1, strlen(data), file);
      return Value::Int((long long)written);
    }},

    {"read", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)std::get<int32>(args[0].v);
      long long size = std::get<int32>(args[1].v);

      if (!file) std::exit(1);
      if (size <= 0) return Value::Str("");

      char* buffer = (char*)malloc(size + 1);
      size_t bytes_read = fread(buffer, 1, size, file);
      buffer[bytes_read] = '\0';

      Value result = Value::Str(buffer);
      free(buffer);
      return result;
    }},

    // FIXME: We don't need to take any arguments
    {"flush", [](std::vector<Value>& args) -> Value {
      FILE* file = (FILE*)(uintptr_t)std::get<int32>(args[0].v);
      if (file) {
        fflush(file);
      }
      return Value::Int(0);
    }},

    // FIXME: We need better type checking
    // FIXME: Add returning multiple values
    {"typeof", [](std::vector<Value>& args) -> Value {
      switch (args[0].t) {
        case Type::Int:    return Value::Str("int");
        case Type::Float:  return Value::Str("float");
        case Type::Str:    return Value::Str("str");
        case Type::Bool:   return Value::Str("bool");
        case Type::List:   return Value::Str("list");
        case Type::Null:   return Value::Str("null");
        case Type::Dict:   return Value::Str("dict");
        case Type::i8:     return Value::Str("i8");
        case Type::i16:    return Value::Str("i16");
        case Type::i32:    return Value::Str("i32");
        case Type::i64:    return Value::Str("i64");
        case Type::ui8:    return Value::Str("ui8");
        case Type::ui16:   return Value::Str("ui16");
        case Type::ui32:   return Value::Str("ui32");
        case Type::ui64:   return Value::Str("ui64");
        case Type::Range:  return Value::Str("range");
        case Type::Void:   return Value::Str("void"); // Safety
        case Type::TriBool:return Value::Str("tribool");
        default: std::exit(1);
      }
    }},

    // FIXME: type checks should join into one
    {"isInt", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::Int);
    }},

    {"isFloat", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::Float);
    }},

    {"isString", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::Str);
    }},

    {"isList", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::List);
    }},

    {"isBool", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::Bool);
    }},

    {"isNull", [](std::vector<Value>& args) -> Value {
      return Value::Bool(args[0].t == Type::Null);
    }}
  };

  struct Env {
    struct Var {
      Type declared;
      Value val;
      Var(Type t, Value v) : declared(t), val(v) {}
    };

    std::unordered_map<std::string, Var> m;
    Env* parent = nullptr;
    Value val;

    explicit Env(Env* p = nullptr) : parent(p), val{} {}

    bool ExistsLocal(const std::string& n) const { return m.find(n) != m.end(); }

    // FIXME: This should be recursive, not limitied to two parent scopes
    bool Exists(const std::string& n) const { return ExistsLocal(n) || (parent && parent->Exists(n)); }

    // FIXME: should not return dummy, we assume it will exist. Compiler handles those errors.
    const Var& Get(const std::string& n) const {
      auto it = m.find(n);
      if (it != m.end()) return it->second;
      if (parent) return parent->Get(n);
      std::exit(1);
    }

    void Declare(const std::string& n, Type t, Value v) {
      m.emplace(n, Var{t, v});
    }

    void Set(const std::string& n, Value v) {
      auto it = m.find(n);
      if (it != m.end()) {
        it->second.val = v;
        return;
      }
      if (parent) {
        parent->Set(n, v);
        return;
      }
      std::exit(1);
    }

    void SetOrDeclare(const std::string& n, Value v) {
      if (ExistsLocal(n))
        Set(n, v);
      else if (parent && parent->Exists(n))
        parent->Set(n, v);
      else
        Declare(n, v.t, v);
    }

    bool Erase(const std::string& n) { return m.erase(n) != 0; }

    bool Unset(const std::string& n) {
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
      size_t stack_base;

      Frame(uint64 rip, std::unique_ptr<Env> e, size_t base)
        : ret_ip(rip), env(std::move(e)), stack_base(base) {}
    };

    std::vector<Frame> frames;

    // FIXME: We don't need to know void or typed at runtime
    struct FnMeta {
      uint64 entry;
      Type ret; // Return values :)
      std::vector<std::string> params;
    };
    std::unordered_map<std::string, FnMeta> fnEntry;

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
    inline std::string GETstr(){
      uint64 n=GETu64();
      if(n == 0) return std::string();

      std::string s(n, '\0');
      fread(&s[0], 1, n, f);
      ip += n;
      return s;
    }

    inline Value pop() {
      try {
        if (stack.empty()) {
          print("FATAL ERROR: Stack underflow. Tried to pop an empty stack.");
          std::exit(1);
        }
        // FIXME: This should use err.hpp (vm_err.hpp)
        if (stack.back().t == Type::Null) {
          print("FATAL ERROR: Stack had null top value.");
          std::exit(1);
        }
        Value v = std::move(stack.back());
        stack.pop_back();
        return v;
      } catch (...) {
        print("FATAL ERROR: Stack operation failed.");
        std::exit(1);
      }
      return Value::Null();
    }

    inline void push(Value v) { stack.push_back(std::move(v)); }

    inline void discard() {
      if (stack.empty()) {
        print("FATAL ERROR: stack underflow; tried to empty an already empty stack");
        std::exit(1);
      }
      stack.pop_back();
    }

    inline void load(const std::string& path) {
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
        std::string name = GETstr();
        uint64 entry = GETu64();
        // FIXME: isVoid & typed aren't needed at runtime
        bool isVoid = GETu8() != 0;
        bool typed = GETu8() != 0;
        Type ret = (Type)GETu8();
        uint64 pcnt = GETu64();
        std::vector<std::string> params;
        params.reserve((size_t)pcnt);
        for (uint64 j = 0; j < pcnt; ++j) params.push_back(GETstr());
        fnEntry[name] = FnMeta{ entry, ret, params };
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
        for (uint64 i = 0; i < plugin_count; i++) {
          std::string module_name = GETstr();
          std::string library_path = GETstr();

          // Load plugin
          if (!PluginManager::load_plugin(module_name, library_path)) {
            print("FATAL ERROR: Failed to load plugin ", module_name, "\n");
            std::exit(1);
          }
        }
      }

      jump(entry_main);
      frames.push_back(Frame{ (uint64)-1, nullptr, 0 });
      frames.back().env = std::make_unique<Env>(&globals);
    }

    inline void run() {
      for (;;) {
        if (ip >= code_end) return;
        unsigned char op = GETu8();
        // FIXME: This needs to possibly lead to another switch, or have switches in the switches.
        switch (op >> 5) {
          case static_cast<int>(Register::LOGIC): {
            switch (op & 0x0f) {
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
                    push(Value::List(std::move(result)));
                  } else {
                    std::vector<Value> result = std::get<std::vector<Value>>(a.v);
                    result.push_back(b);
                    push(Value::List(std::move(result)));
                  }
                } else if (a.t == Type::Str || b.t == Type::Str) {
                  std::string result = std::get<std::string>(a.v) + std::get<std::string>(b.v);
                  push(Value::Str(std::move(result)));
                } else if (a.t == Type::Float || b.t == Type::Float) {
                  push(Value::Float(std::get<double>(a.v) + std::get<double>(b.v)));
                } else if (a.t == Type::Int || b.t == Type::Int) {
                  push(Value::Int(std::get<int32>(a.v) + std::get<int32>(b.v)));
                }
              } break;
              case static_cast<int>(Logic::EQUAL): {
                Value b = pop(), a = pop();
                bool eq = (a.t == b.t) ? (a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (std::get<double>(a.v) == std::get<double>(b.v)) : false);
                push(Value::Bool(eq));
              } break;
              case static_cast<int>(Logic::SUBTRACT): {
                Value b = pop(), a = pop();
                if ((a.t == Type::Int || a.t == Type::Float) && (b.t == Type::Int || b.t == Type::Float)) {
                  if (a.t == Type::Float || b.t == Type::Float) {
                    push(Value::Float(std::get<double>(a.v) - std::get<double>(b.v)));
                  } else {
                    push(Value::Int(std::get<int32>(a.v) - std::get<int32>(b.v)));
                  }
                }
              } break;

              case static_cast<int>(Logic::MULTIPLY): {
                Value b = pop(), a = pop();
                if (a.t == Type::Float || b.t == Type::Float)
                  push(Value::Float(std::get<double>(a.v) * std::get<double>(b.v)));
                else
                  push(Value::Int(std::get<int32>(a.v) * std::get<int32>(b.v)));
              } break;
              case static_cast<int>(Logic::DIVIDE): {
                Value b = pop(), a = pop();
                push(Value::Float(std::get<double>(a.v) / std::get<double>(b.v)));
              } break;

              case static_cast<int>(Logic::JUMP): { uint64 tgt = GETu64(); jump(tgt); } break;
              case static_cast<int>(Logic::JUMP_IF_FALSE):  { uint64 tgt = GETu64(); Value v = pop(); if (!std::get<bool>(v.v)) jump(tgt); } break; // Jump if false
              case static_cast<int>(Logic::AND): { Value b = pop(), a = pop(); push(Value::Bool(std::get<bool>(a.v) && std::get<bool>(b.v))); } break;
              case static_cast<int>(Logic::OR):  { Value b = pop(), a = pop(); push(Value::Bool(std::get<bool>(a.v) || std::get<bool>(b.v))); } break;
              case static_cast<int>(Logic::LESS_OR_EQUAL): {
                Value b = pop(), a = pop();
                if (a.t == Type::Str && b.t == Type::Str)
                  push(Value::Bool(std::get<std::string>(a.v) <= std::get<std::string>(b.v)));
                else
                  push(Value::Bool(std::get<double>(a.v) <= std::get<double>(b.v)));
              } break;

              case static_cast<int>(Logic::LESS_THAN): {
                Value b = pop(), a = pop();
                if (a.t == Type::Str && b.t == Type::Str)
                  push(Value::Bool(std::get<std::string>(a.v) < std::get<std::string>(b.v)));
                else
                  push(Value::Bool(std::get<double>(a.v) < std::get<double>(b.v)));
              } break;
              case static_cast<int>(Logic::NOT_EQUAL): {
                Value b = pop(), a = pop();
                bool ne = (a.t == b.t) ? !(a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (std::get<double>(a.v) != std::get<double>(b.v)) : true);
                push(Value::Bool(ne));
              } break;
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
                    case 0x08: push(Value::Float(GETd64())); break;
                    case 0x09: {
                      unsigned char bool_val = meta & 0b00000100;
                      if (!(meta && 0b00000000)) {
                        push(Value::Bool(false));
                      } else {
                        push(Value::Bool(true));
                      }
                    } break;
                    default: {
                      print("FATAL ERROR: Unknown meta tag");
                      std::exit(1);
                    }
                  }
                  // FIXME: Add tagged signedness, so every int in the lang has the ability to be
                  // 64-bit unsigned, even if signed
                  /*type = meta;
                  type |= 0b00001000;
                  switch(type) {
                    case 0x00: // Signed
                    case 0x01: // Unsigned
                  }*/
                } break;

                case 3: push(Value::Str(GETstr())); break;
                case 4: {
                  uint64 n = GETu64();
                  std::vector<Value> xs; xs.resize(n);
                  for (uint64 i = 0; i < n; ++i) xs[n-1-i] = pop();
                  push(Value::List(std::move(xs)));
                } break;
                default: {
                  print("unknown literal type tag");
                  std::exit(1);
                }
              }
            } break;
            case static_cast<int>(Variable::SET): frames.back().env->SetOrDeclare(GETstr(), pop()); break;
            case static_cast<int>(Variable::DECLARE): {
              std::string id = GETstr();
              // FIXME:
              uint64 tt = GETu64();
              Value v  = pop();
              if (tt == 0xECull) frames.back().env->Declare(id, v.t, v);
              else               frames.back().env->Declare(id, (Type)tt, v);
            } break;
            case static_cast<int>(Variable::GET): {
              std::string id = GETstr();
              push(frames.back().env->Get(id).val);
            } break;

            // FIXME: This is possibly incomplete
            case static_cast<int>(Variable::UNSET): {
              std::string id = GETstr();
              if (!frames.back().env->Unset(id)) {}
            } break;
          } break;


          case static_cast<int>(Register::GENERAL): {
            switch (op & 0x1F) {
              case static_cast<int>(General::HALT): return;
              case static_cast<int>(General::NOP): break;
              case static_cast<int>(General::POP): discard(); break;
              case static_cast<int>(General::YIELD): {
                #ifdef _WIN32
                  _getch();
                #else
                  system("read -n 1");
                #endif
              } break;
              case static_cast<int>(General::INDEX): {
                Value idxV = pop();
                Value base = pop();
                long long i = std::get<int32>(idxV.v);
                if (base.t == Type::List) {
                  // FIXME: Prefer explicit over auto
                  auto& xs = std::get<std::vector<Value>>(base.v);
                  push(xs[(size_t)i]);
                } else if (base.t == Type::Str) {
                  const std::string& s = std::get<std::string>(base.v);
                  size_t len = s.length();
                  if (i < 0 || (size_t)i >= len) {
                    print("FATAL ERROR: Index out of range. Attempt to get item in list or string that doesn't exist.");
                  }
                  push(Value::Str(std::string(1, s[i])));
                }
              } break;
            }
          } break;
          case static_cast<int>(Register::FUNCTION): {
            switch (op & 0x1F) {
              // FIXME: Could be simpler to implement via other MVME opcodes
              case static_cast<int>(Func::TAIL): {
              std::string name = GETstr();
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

              Frame& currentFrame = frames.back();

              Env* callerEnv = frames[frames.size()-2].env.get();
              currentFrame.env = std::make_unique<Env>(callerEnv);

              for (size_t i = 0; i < meta.params.size() && i < args.size(); ++i) {
                // FIXME: We might be able to use just args[i] then do .t in the function
                currentFrame.env->Declare(meta.params[i], args[i].t, args[i]);
              }

              jump(meta.entry);
              } break;

              case static_cast<int>(Func::RETURN): {
                Value rv;
                if (stack.size() > frames.back().stack_base) {
                  rv = pop();
                } else {
                  rv = Value::Void();
                }
                if (frames.size() == 1) return;
                uint64 ret = frames.back().ret_ip;
                size_t caller_base = frames.back().stack_base;
                frames.pop_back();

                while (stack.size() > caller_base) {
                  stack.pop_back();
                }

                jump(ret);
                push(rv);
              } break;

              case static_cast<int>(Func::CALL): {
                std::string name = GETstr();
                uint64 argc = GETu64();
                std::vector<Value> args(argc);
                for (size_t i = 0; i < argc; ++i) args[argc-1-i] = pop();

                auto it = fnEntry.find(name);
                if (it == fnEntry.end()) {
                  auto bit = builtins.find(name);
                  if (bit == builtins.end()) {
                    // Check plugin functions
                    auto pfn = PluginManager::get_functions(name);
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

                frames.push_back(Frame{ ip, nullptr, stack.size() });
                Env* callerEnv = frames[frames.size()-2].env.get();
                frames.back().env = std::make_unique<Env>(callerEnv);

                for (size_t i = 0; i < meta.params.size() && i < args.size(); ++i) {
                  frames.back().env->Declare(meta.params[i], args[i].t, args[i]);
                }

                jump(meta.entry);
              } break;
            }
          } break;
          case static_cast<int>(Register::IMPORT): {

          } break;
          default: {
            print("FATAL ERROR: Bad or unknown opcode.");
            std::exit(1);
          }
        }
      }
    }
    }
  };

  VM::VM() = default;
  VM::~VM() = default;

  void VM::load(const std::string& path) {
    engine = std::make_unique<VMEngine>();
    engine->load(path);
  }

  void VM::run() {
    if (engine) {
      engine->run();
    }
  }

  // Global run function
  void run(const std::string& path) {
    VMEngine vm;
    vm.load(path);
    vm.run();
  }
}
