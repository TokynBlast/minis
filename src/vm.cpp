#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <string_view>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <set>
#include <array>
#if _WIN32
  #include <conio.h> // Provides Windows _getch()
#endif
#include "../include/bytecode.hpp"
#include "../include/types.hpp"
#include "../include/value.hpp"
#include "../include/vm.hpp"
#include "../include/plugin.hpp"
#include "../include/macros.h"
#include "../include/io.hpp"
// #include <fast_io.h>
#include "../fast_io/include/fast_io.h"


// Fortran math functions from src/maths.f08
extern "C" {
  // The following functions server the purpose of using Fortran for vectorization

  // Multi-add functions
  inline int8 divide_multi_i8(const int8* values, uint64 n);
  inline int16 divide_multi_i16(const int16 *values, uint64 n);
  inline int32 divide_multi_i32(const int32 *values, uint64 n);
  inline int64 divide_multi_i64(const int64 *values, uint64 n);
  inline uint8 divide_multi_ui8(const uint8 *values, uint64 n);
  inline uint16 divide_multi_ui16(const uint16 *values, uint64 n);
  inline uint32 divide_multi_ui32(const uint32 *values, uint64 n);
  inline uint64 divide_multi_ui64(const uint64 *values, uint64 n);

  // Multi-multiply functions
  inline int8 multiply_multi_i8(const int8 *values, uint64 n);
  inline int16 multiply_multi_i16(const int16 *values, uint64 n);
  inline int32 multiply_multi_i32(const int32 *values, uint64 n);
  inline int64 multiply_multi_i64(const int64 *values, uint64 n);
  inline uint8 multiply_multi_ui8(const uint8 *values, uint64 n);
  inline uint16 multiply_multi_ui16(const uint16 *values, uint64 n);
  inline uint32 multiply_multi_ui32(const uint32 *values, uint64 n);
  inline uint64 multiply_multi_ui64(const uint64 *values, uint64 n);

  // Multi-divide functions
  inline int8 divide_multi_i8(const int8 *values, uint64 n);
  inline int16 divide_multi_i16(const int16 *values, uint64 n);
  inline int32 divide_multi_i32(const int32 *values, uint64 n);
  inline int64 divide_multi_i64(const int64 *values, uint64 n);
  inline uint8 divide_multi_ui8(const uint8 *values, uint64 n);
  inline uint16 divide_multi_ui16(const uint16 *values, uint64 n);
  inline uint32 divide_multi_ui32(const uint32 *values, uint64 n);
  inline uint64 divide_multi_ui64(const uint64 *values, uint64 n);

  // Multi-subtract functions
  inline int8 subtract_multi_i8(const int8 *values, uint64 n);
  inline int16 subtract_multi_i16(const int16 *values, uint64 n);
  inline int32 subtract_multi_i32(const int32 *values, uint64 n);
  inline int64 subtract_multi_i64(const int64 *values, uint64 n);
  inline uint8 subtract_multi_ui8(const uint8 *values, uint64 n);
  inline uint16 subtract_multi_ui16(const uint16 *values, uint64 n);
  inline uint32 subtract_multi_ui32(const uint32 *values, uint64 n);
  inline uint64 subtract_multi_ui64(const uint64 *values, uint64 n);

  // Multi-double-related functions
  inline double divide_multi_f64(const double *values, uint64 n);
  inline double multiply_multi_f64(const double *values, uint64 n);
  inline double divide_multi_f64(const double *values, uint64 n);
  inline double subtract_multi_f64(const double *values, uint64 n);
}
// fast_io is here to replace standard iostream
// until one of the following occurs:
// iostream gets replaced or improved
// fast_io is depreceated
// something better is found

using fast_io::io::print;
using fast_io::io::scan;
using fast_io::io::perr;

namespace minis {
  /*=============================
  =          Values/Env         =
  =============================*/

  // Built-in function handler signature
  using BuiltinFn = std::function<Value(std::vector<Value>&)>;

  // Registry of built-in functions
  // FIXME: Instead of storing every function as a string, we should store it some other way,
  // like an enum or computed goto, to improve speed.
  inline static std::unordered_map<std::string, BuiltinFn> builtins = {
      // FIXME: We need to add the ability to add the end line manually.
      // FIXME: We need to make print more customizable
      {"print", [](std::vector<Value> &args) -> Value {
        auto print_value = [&](const Value& val, auto&& print_value_ref) -> void {
          switch (val.t) {
            case Type::Float: print(std::get<double>(val.v)); break;
            case Type::Str:   print(std::get<std::string>(val.v)); break;
            case Type::Bool:  print((bool)std::get<bool>(val.v)); break;
            case Type::Null:  print("null"); break;
            case Type::i8:    print(std::get<int8>(val.v)); break;
            case Type::i16:   print(std::get<int16>(val.v)); break;
            case Type::i32:   print(std::get<int32>(val.v)); break;
            case Type::i64:   print(std::get<int64>(val.v)); break;
            case Type::ui8:   print(std::get<uint8>(val.v)); break;
            case Type::ui16:  print(std::get<uint16>(val.v)); break;
            case Type::ui32:  print(std::get<uint32>(val.v)); break;
            case Type::ui64:  print(std::get<uint64>(val.v)); break;
            case Type::List: {
              const auto& list = std::get<std::vector<Value>>(val.v);
              print("[");
              for (size_t j = 0; j < list.size(); ++j) {
                print_value_ref(list[j], print_value_ref);
                if (j + 1 < list.size()) print(", ");
              }
              print("]");
              break;
            }
            case Type::Dict: print("[Dict]"); break; // TODO: pretty print dicts
            case Type::Void: print("Error :("); exit(1);
            case Type::TriBool: /* TODO: handle tribool */ break;
            default: print("FATAL ERROR: Unknown type ", (uint8)val.t); exit(1);
          }
        };

        size_t arg_amnt = args.size();
        for (size_t i = 0; i < arg_amnt; i++) {
          print_value(args[i], print_value);
          if (i + 1 < arg_amnt) print(" ");
        }
        // FIXME: print should return nothing
        return Value::Void();
      }},
      {"abs", [](std::vector<Value> &args) {
        auto val = args[0];
        if (val.t == Type::Float)
          return Value::Float(std::abs(std::get<double>(val.v)));
        return Value::I64(std::abs(std::get<int64>(val.v)));
      }},
      {"neg", [](std::vector<Value> &args) {
        auto &arg = args[0];
        if (arg.t == Type::Float)
          return Value::Float(-std::get<double>(arg.v));
        // FIXME: Should return same size
        return Value::I64(-std::get<int64>(arg.v));
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
      {"range", [](std::vector<Value> &args) {
         uint64 start = 0, end;
         if (args.size() == 1) {
           end = std::get<uint64>(args[0].v);
         } else {
           start = std::get<uint64>(args[0].v);
           end = std::get<uint64>(args[1].v);
         }

         std::map<uint64, uint64> range_map;
         range_map[0] = start; // Store start at key 0
         range_map[1] = end;   // Store end at key 1
         return Value::Range(range_map);
       }},
      // FIXME: Should return Value types
      {"max", [](std::vector<Value> &args) {
         Value max = args[0];
         for (size_t i = 1; i < args.size(); i++)
         {
           if (args[i] > max)
             max = args[i];
         }
         return max;
       }},
      {"min", [](std::vector<Value> &args) -> Value {
        Value min = args[0];
        for (size_t i = 1; i < args.size(); i++) {
          if (args[i] < min)
            min = args[i];
        }
        return min;
      }},
      {"sort", [](std::vector<Value> &args) {
         std::vector<Value> list = std::get<std::vector<Value>>(args[0].v);
         std::sort(list.begin(), list.end(),
                   [](const Value &a, const Value &b)
                   { return std::get<double>(a.v) < std::get<double>(b.v); });
         return Value::List(list);
       }},
      {"reverse", [](std::vector<Value> &args) -> Value {
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
      // FIXME:Use Fortran backend instead of C++s
      {"sum", [](std::vector<Value> &args) {
         const auto &list = std::get<std::vector<Value>>(args[0].v);
         Value sum = Value::I32(0);
         for (const auto &v : list)
         {
           if (v.t == Type::Float)
             sum = Value::Float(std::get<double>(sum.v) + std::get<double>(v.v));
           else sum = Value::I32(std::get<int32>(sum.v) + std::get<int32>(v.v));
         }
         return sum;
       }},
      // Print like print, using all values
      {"input", [](std::vector<Value> &args) -> Value {
        std::string input;
        if (!args.empty()) {
          print(""); // print the values in here :)
                     // maybe run built-in print function?
        }
        scan(input);
        return Value::Str(std::move(input));
      }},
      {"len", [](std::vector<Value> &args) -> Value {
        const auto &arg = args[0];
        if (arg.t == Type::List) {
          return Value::UI64(static_cast<uint64>(std::get<std::vector<Value>>(arg.v).size()));
        } else if (arg.t == Type::Str) {
          return Value::UI64(static_cast<uint64>(std::get<std::string>(arg.v).length()));
        }
        return Value::Null();
       }},
      {"split", [](std::vector<Value> &args) -> Value {
        const std::string &str = std::get<std::string>(args[0].v);
        const std::string &delim = std::get<std::string>(args[1].v);

        std::vector<Value> result;
        std::string s(str);
        std::string delimiter(delim);

        size_t pos = 0;
        while ((pos = s.find(delimiter)) != std::string::npos) {
          result.push_back(Value::Str(s.substr(0, pos).c_str()));
          s.erase(0, pos + delimiter.length());
        }
        result.push_back(Value::Str(s.c_str()));

        return Value::List(std::move(result));
      }},
      {"upper", [](std::vector<Value> &args) -> Value {
        // >> 30 :) // ANSI only
        std::string result = std::get<std::string>(args[0].v);
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return Value::Str(std::move(result));
      }},
      {"lower", [](std::vector<Value> &args) -> Value {
        std::string result = std::get<std::string>(args[0].v);
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return Value::Str(std::move(result));
      }},
      {"round", [](std::vector<Value> &args) -> Value {
        return Value::I64((int64)std::round(std::get<double>(args[0].v)));
      }},

      // Implement via Fortran and C++
      // {"random", [](std::vector<Value> &args) -> Value {
      //   // use random lib instead
      //   if (args.empty()) {
      //     return Value::Float((double)rand() / RAND_MAX);
      //   } else {
      //     uint64 max_val = std::get<uint64>(args[0].v);
      //     return Value::UI64(rand() % max_val);
      //   }
      // }},

      // FIXME: Use fast_io :)
      // {"open", [](std::vector<Value> &args) -> Value {
      //   const char *filename = std::get<std::string>(args[0].v).c_str();
      //   const char *mode_str = std::get<std::string>(args[1].v).c_str();
      //   // FIXME: This should be a switch instead
      //   // Translate Minis modes to C modes
      //   const char *c_mode = "r";
      //   // FIXME: Simplify to normality
      //   if (strcmp(mode_str, "r") == 0) c_mode = "r";
      //   else if (strcmp(mode_str, "rb") == 0) c_mode = "rb";
      //   else if (strcmp(mode_str, "rs") == 0) c_mode = "r"; // read specific = read but read only a section, rather than put the whole file into mem
      //   else if (strcmp(mode_str, "w") == 0) c_mode = "w";
      //   else if (strcmp(mode_str, "wb") == 0) c_mode = "wb";
      //   else if (strcmp(mode_str, "ws") == 0) c_mode = "w"; // write specific = write but write to a specific area rather than rewrite the whole thing
      //   /*write targeted = append but write anywhere, not just the end, shifting alldata after up
      //    * For example:
      //    * appending ", " to five (5) in "Helloworld" makes it "Hello, world"
      //    */
      //   else if (strcmp(mode_str, "wt") == 0) c_mode = "a";
      //   else if (strcmp(mode_str, "a") == 0) c_mode = "a"; // also support append directly

      //   FILE *file = fopen(filename, c_mode);
      //   if (!file) return Value::Bool(1); // Error code

      //   // Store file pointer as integer (simple approach)
      //   return Value::UI64((uint64)(uintptr_t)file);
      //  }},

      // {"close", [](std::vector<Value> &args) -> Value {
      //   FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
      //   if (file && file != stdin && file != stdout && file != stderr)
      //   {
      //     fclose(file);
      //   }
      //    return Value::Bool(false);
      //  }},

      // {"write", [](std::vector<Value> &args) -> Value {
      //    FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
      //    const char *data = std::get<std::string>(args[1].v).c_str();

      //    if (!file) std::exit(1);

      //    size_t written = fwrite(data, 1, strlen(data), file);
      //    return Value::Bool((bool)written);
      //  }},

      {"read", [](std::vector<Value> &args) -> Value {
        try {
          std::string filename = std::get<std::string>(args[0].v);
          fast_io::native_file_loader loader(filename);

          // Create the string from the loader data immediately
          std::string filedata(reinterpret_cast<const char*>(loader.data()), loader.size());

          // 1. Remove UTF-8 BOM if present (Crucial for preventing garbled starts)
          if (filedata.size() >= 3 &&
            (unsigned char)filedata[0] == 0xEF &&
            (unsigned char)filedata[1] == 0xBB &&
            (unsigned char)filedata[2] == 0xBF) {
            filedata = filedata.substr(3);
          }

          // 2. Return the processed string
          return Value::Str(std::move(filedata));

        } catch (const std::exception& e) {
          print("File I/O error: ", std::string(e.what()), "\n");
          std::exit(1);
        }
      }},

      // FIXME: We don't need to take any arguments
      // {"flush", [](std::vector<Value> &args) -> Value {
      //    FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
      //    if (file)
      //    {
      //      fflush(file);
      //    }
      //    return Value::UI8(0);
      //  }},

      // FIXME: We need better type checking
      // FIXME: Add returning multiple values
      {"typeof", [](std::vector<Value> &args) -> Value {
          switch (args[0].t) {
            case Type::Float:   return Value::Str("float");
            case Type::Str:     return Value::Str("str");
            case Type::Bool:    return Value::Str("bool");
            case Type::List:    return Value::Str("list");
            case Type::Null:    return Value::Str("null");
            case Type::Dict:    return Value::Str("dict");
            case Type::i8:      return Value::Str("i8");
            case Type::i16:     return Value::Str("i16");
            case Type::i32:     return Value::Str("i32");
            case Type::i64:     return Value::Str("i64");
            case Type::ui8:     return Value::Str("ui8");
            case Type::ui16:    return Value::Str("ui16");
            case Type::ui32:    return Value::Str("ui32");
            case Type::ui64:    return Value::Str("ui64");
            case Type::Range:   return Value::Str("range");
            case Type::Void:    return Value::Str("void"); // Safety
            case Type::TriBool: return Value::Str("tribool");
            default: {
              print("Unknown type\n");
              std::exit(1);
            }
          }
       }},
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

    void Declare(const std::string& n, Value v) {
      m.emplace(n, Var{v.t, v});
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
        Declare(n, v);
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
      // FIXME: Return types can be 100% compile-time
      //Type ret;
      std::vector<std::string> params;
    };
    std::unordered_map<std::string, FnMeta> fnEntry;

    struct DebugInfo {
      std::string filename;
      std::map<uint64, uint32> offset_to_line;
      std::map<uint64, std::string> offset_to_function;
    };
    DebugInfo debug_info;

    explicit VMEngine() {}
    ~VMEngine() { if (f) fclose(f); }

    inline void jump(uint64 target) {
      ip = target; fseek(f, (uint64)ip, SEEK_SET);
    }

    // unsigned ints
    inline uint8 GETu8() {
      uint8 v;
      fread(&v, 1, 1, f);
      ip++;
      return v;
    }

    inline uint16 GETu16() {
      uint16 v;
      fread(&v, 2, 1, f);
      ip+=2;
      return v;
    }
    inline uint32 GETu32() {
      uint32 v;
      fread(&v, 4, 1, f);
      ip+=4;
      return v;
    }

    inline uint64 GETu64() {
      uint64 v;
      fread(&v, 8, 1, f);
      ip+=8;
      return v;
    }

    // signed ints
    inline int8 GETs8() {
      int8 v;
      fread(&v, 1, 1, f);
      ip++;
      return v;
    }

    inline int8 GETs16() {
      int16 v;
      fread(&v, 2, 1, f);
      ip+=2;
      return v;
    }

    inline int8 GETs32() {
      int32 v;
      fread(&v, 4, 1, f);
      ip+=4;
      return v;
    }

    inline int64 GETs64() {
      int64 v;
      fread(&v, 8, 1, f);
      ip += 8;
      return v;
    }

    inline double GETd64() {
      double v;
      fread(&v, 8, 1, f);
      ip += 8;
      return v;
    }

    inline std::string GETstr() {
      // FIXME: simplify debugger version
      uint64 n=GETu64();
      if(n == 0) {
        return std::string();
      }

      std::string s(n, '\0');
      fread(&s[0], 1, n, f);
      ip += n;
      return s;
    }

    inline Value pop() {
      try {
        if (stack.empty()) {
          print("FATAL ERROR: Stack underflow; Tried to pop an empty stack\n");
          perr_debug_location_if_available();
          std::exit(1);
        }
        if (stack.back().t == Type::Null) {
          print("FATAL ERROR: Stack had null top value\n");
          perr_debug_location_if_available();
          std::exit(1);
        }
        if (stack.back().t == Type::Void) {
          print("FATAL ERROR: Stack had void top value\n");
          perr_debug_location_if_available();
          std::exit(1);
        }
        Value v = std::move(stack.back());
        stack.pop_back();
        return v;
      } catch (...) {
        print("FATAL ERROR: Stack operation failed\n");
        perr_debug_location_if_available();
        std::exit(1);
      }
      return Value::Null(); // impossible to reach :)
                            // we can use GNU impossible :3
    }

    inline void push(Value v) { stack.push_back(std::move(v)); }

    inline void discard() {
      if (stack.empty()) {
        print("FATAL ERROR: stack underflow; tried to empty an already empty stack\n");
        perr_debug_location_if_available();
        std::exit(1);
      }
      stack.pop_back();
    }

    inline const char* type_name(Type t) {
      switch (t) {
        case Type::Float:   return "float";
        case Type::Str:     return "str";
        case Type::Bool:    return "bool";
        case Type::List:    return "list";
        case Type::Null:    return "null";
        case Type::Dict:    return "dict";
        case Type::i8:      return "i8";
        case Type::i16:     return "i16";
        case Type::i32:     return "i32";
        case Type::i64:     return "i64";
        case Type::ui8:     return "ui8";
        case Type::ui16:    return "ui16";
        case Type::ui32:    return "ui32";
        case Type::ui64:    return "ui64";
        case Type::Range:   return "range";
        case Type::Void:    return "void";
        case Type::TriBool: return "tribool";
        default:            return "unknown";
      }
    }

    inline std::string get_debug_location() {
      if (debug_info.offset_to_line.empty()) {
        return "unknown location";
      }

      auto it = debug_info.offset_to_line.upper_bound(ip);
      if (it != debug_info.offset_to_line.begin()) {
        --it;
        std::string loc = debug_info.filename + ":" + std::to_string(it->second);

        auto fn_it = debug_info.offset_to_function.upper_bound(ip);
        if (fn_it != debug_info.offset_to_function.begin()) {
          --fn_it;
          loc += " in " + fn_it->second + "()";
        }

        return loc;
      }

      return debug_info.filename + ":?";
    }

    inline void perr_debug_location_if_available() {
      if (!debug_info.offset_to_line.empty()) {
        perr("  at ", get_debug_location(), "\n");
      }
    }

    static std::set<std::string> loaded_plugins;
    static std::set<std::string> loaded_libs;
    static uint8 lib_recursion_depth = 0;
    static const uint8 MAX_LIB_RECURSION = 32;

    inline void load(const std::string& path, bool is_lib = false) {
      // Check for library cycles
      if (is_lib) {
        if (loaded_libs.count(path)) return;

        lib_recursion_depth++;
        if (lib_recursion_depth > MAX_LIB_RECURSION) {
          throw std::runtime_error("library recursion depth exceeded\n");
        }
      }

      FILE* file = fopen(path.c_str(), "rb");
      if (!file) throw std::runtime_error("cannot open bytecode\n");

      char magic[9] = {0};
      fread(magic, 1, 8, file);
      if (std::memcmp(magic, "  \xc2\xbd" "6e" "\xc3\xa8", 8) != 0) {
        fclose(file);
        throw std::runtime_error("bad bytecode verification\n");
      }

      // Activation bits
      // 00000001 = debugging
      // 00000010 = plugins
      // 00000100 = functions
      // rest unused
      uint8 activation_bits = GETu8();

      uint64 entry_main = GETu64();
      table_off = GETu64();

      // Read conditional table offsets based on activation bits
      uint64 debug_table_off = 0;
      uint64 plugin_table_off = 0;
      uint64 function_table_off = 0;

      if (activation_bits & 0b00000001) {
        debug_table_off = GETu64();
      }
      if (activation_bits & 0b00000010) {
        plugin_table_off = GETu64();
      }
      if (activation_bits & 0b00000100) {
        function_table_off = GETu64();
      }

      uint64 lib_table_off = GETu64();

      // Header is exactly 40 bytes (8 magic + 32 data)

      // Mark library as loaded BEFORE recursing
      if (is_lib) {
        loaded_libs.insert(path);
      } else {
        // For main program
        ip = entry_main;
        code_end = table_off;
      }

      // Load function table if present
      if (function_table_off > 0) {
        fseek(file, (uint64)function_table_off, SEEK_SET);
        uint64 fn_count = GETu64();

        for (uint64 i = 0; i < fn_count; i++) {
          std::string name = GETstr();
          uint64 entry = GETu64();
          uint64 param_count = GETu64();

          std::vector<std::string> params;
          params.reserve((size_t)param_count);
          for (uint64 j = 0; j < param_count; ++j) {
            params.push_back(GETstr());
          }

          fnEntry[name] = FnMeta{ entry, params };
        }
      }

      // Load libraries if lib table exists
      if (lib_table_off > 0) {
        fseek(file, (uint64)lib_table_off, SEEK_SET);
        uint64 lib_count = GETu64();

        for (uint64 i = 0; i < lib_count; i++) {
          std::string lib_name = GETstr();
          bool has_custom_path = GETu8();
          std::string lib_path;

          if (has_custom_path) {
            lib_path = GETstr();
          } else {
            lib_path = "./libs/" + lib_name + ".vbc";
          }

          // Recursively load library
          load(lib_path, true);
        }
      }

      // Load plugins if plugin table exists
      if (plugin_table_off > 0) {
        fseek(file, (uint64)plugin_table_off, SEEK_SET);
        uint64 plugin_count = GETu64();

        for (uint64 i = 0; i < plugin_count; i++) {
          std::string module_name = GETstr();

          // Skip if already loaded
          if (loaded_plugins.count(module_name)) continue;

          bool has_custom_path = GETu8();
          std::string library_path;

          if (has_custom_path) {
            library_path = GETstr();
          } else {
            #if defined(_WIN32) || defined(_WIN64)
              library_path = "./plugins/" + module_name + ".dll";
            #elif defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
              library_path = "./plugins/" + module_name + ".so";
            #endif
          }

          if (!PluginManager::load_plugin(module_name, library_path)) {
            fclose(file);
            print("FATAL ERROR: Failed to load plugin ", module_name, ", ", library_path, " does not exist\n");
            std::exit(1);
          }

          loaded_plugins.insert(module_name);
        }
      }

      // Load debug table if present
      if (debug_table_off > 0) {
        fseek(file, (uint64)debug_table_off, SEEK_SET);

        debug_info.filename = GETstr();

        uint64 line_map_count = GETu64();
        for (uint64 i = 0; i < line_map_count; i++) {
          uint64 offset = GETu64();
          uint32 line = GETu32();
          debug_info.offset_to_line[offset] = line;
        }

        uint64 fn_map_count = GETu64();
        for (uint64 i = 0; i < fn_map_count; i++) {
          uint64 offset = GETu64();
          std::string fn_name = GETstr();
          debug_info.offset_to_function[offset] = fn_name;
        }
      }

      fclose(file);

      // Decrement recursion depth
      if (is_lib) {
        lib_recursion_depth--;
      }

      // Only initialize for main program
      if (!is_lib) {
        jump(entry_main);
        frames.push_back(Frame{ (uint64)-1, nullptr, 0 });
        frames.back().env = std::make_unique<Env>(&globals);
      }
    }

    inline void run() {
      for (;;) {
        if (ip >= code_end) return;
        unsigned char op = GETu8();
        switch (op >> 5) {
          case static_cast<uint8>(Register::LOGIC): {
            switch (op & 0x1F) {
              case static_cast<uint8>(Logic::NOT): {
                Value a = pop();
                if (a.t != Type::Bool) {
                  perr("FATAL ERROR: Not (!= or !) requires boolean operand\n");
                  perr_debug_location_if_available();
                  perr("  got type: ", type_name(a.t), "\n");
                  exit(1);
                }
                push(Value::Bool(!std::get<bool>(a.v)));
              }
              case static_cast<uint8>(Logic::EQUAL): {
                Value a = pop(), b = pop();
                bool eq = (a.t == b.t) ? (a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (std::get<double>(a.v) == std::get<double>(b.v)) : false);
                push(Value::Bool(eq));
              } break;

              case static_cast<uint8>(Logic::JUMP): {
                jump(GETu64());
              } break;
              case static_cast<uint8>(Logic::JUMP_IF): {
                Value v = pop();
                if (std::get<bool>(v.v)) {
                  jump(GETu64());
                }
              } break;
              case static_cast<uint8>(Logic::JUMP_IF_NOT): {
                uint64 tgt = GETu64();
                Value v = pop();
                if (!std::get<bool>(v.v)) {
                  jump(tgt);
                }
              } break;
              case static_cast<uint8>(Logic::AND): {
                Value a = pop(), b = pop();
                push(
                  Value::Bool(
                    std::get<bool>(a.v)
                    &&
                    std::get<bool>(b.v)
                  )
                );
              } break;
              case static_cast<uint8>(Logic::OR): {
                Value a = pop(), b = pop();
                push(
                  Value::Bool(
                    std::get<bool>(a.v)
                    ||
                    std::get<bool>(b.v)
                  )
                );
              } break;
              case static_cast<uint8>(Logic::LESS_OR_EQUAL): {
                Value a = pop(), b = pop();
                if (a.t == Type::Float)
                  push(Value::Bool(std::get<double>(a.v) <= std::get<double>(b.v)));
                else if (a.t == Type::i8 || a.t == Type::i16 || a.t == Type::i32 || a.t == Type::i64)
                  push(Value::Bool(std::get<int64>(a.v) <= std::get<int64>(b.v)));
                else
                  push(Value::Bool(std::get<uint64>(a.v) <= std::get<uint64>(b.v)));
              } break;

              case static_cast<uint8>(Logic::LESS_THAN): {
                Value a = pop(), b = pop();
                if (a.t == Type::Float)
                  push(Value::Bool(std::get<double>(a.v) < std::get<double>(b.v)));
                else if (a.t == Type::i8 || a.t == Type::i16 || a.t == Type::i32 || a.t == Type::i64)
                  push(Value::Bool(std::get<int64>(a.v) < std::get<int64>(b.v)));
                else
                  push(Value::Bool(std::get<uint64>(a.v) < std::get<uint64>(b.v)));
              } break;
              case static_cast<uint8>(Logic::NOT_EQUAL): {
                Value a = pop(), b = pop();
                bool ne = (a.t == b.t) ? !(a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (std::get<double>(a.v) != std::get<double>(b.v)) : true);
                push(Value::Bool(ne));
              } break;
            }
          } break;
          case static_cast<uint8>(Register::MATH): {
            switch (op & 0x1F) {
              // FIXME: Use Fortran functions
              /*case static_cast<uint8>(Math::SUB): {
                Value a = pop(), b = pop();
                if ((a.t == Type::Int || a.t == Type::Float) && (b.t == Type::Int || b.t == Type::Float)) {
                  if (a.t == Type::Float || b.t == Type::Float) {
                    push(Value::Float(std::get<double>(a.v) - std::get<double>(b.v)));
                  } else {
                    push(Value::I64(std::get<int64>(a.v) - std::get<int64>(b.v)));
                  }
                }
              } break;*/
              case static_cast<uint8>(Math::MULT): {
                // FIXME: Make it so power of two shifts to left
                Value a = pop(), b = pop();
                if (a.t == Type::Float || b.t == Type::Float)
                  push(Value::Float(std::get<double>(a.v) * std::get<double>(b.v)));
                else
                  push(Value::I64(std::get<int64>(a.v) * std::get<int64>(b.v)));
              } break;
              case static_cast<uint8>(Math::DIV): {
                Value a = pop(), b = pop();
                push(Value::Float(std::get<double>(a.v) / std::get<double>(b.v)));
              } break;
              case static_cast<uint8>(Math::ADD): {
                Value a = pop(), b = pop();
                // FIXME: This needs to be tested for bugs heavily; When it was found for sorting,
                //        I found the first if as an else if.
                if (a.t == Type::List) {
                  std::vector<Value> result = std::get<std::vector<Value>>(a.v);
                  // If b is a list, concatenate; else, append b to the list
                  if (b.t == Type::List) {
                    const auto& R = std::get<std::vector<Value>>(b.v);
                    result.insert(result.end(), R.begin(), R.end());
                  } else {
                    result.push_back(b);
                  }
                  push(Value::List(std::move(result)));
                } else if (b.t == Type::List) {
                  // If b is a list, append a to the front of b
                  std::vector<Value> result = std::get<std::vector<Value>>(b.v);
                  result.insert(result.begin(), a);
                  push(Value::List(std::move(result)));
                } else if (a.t == Type::Str || b.t == Type::Str) {
                  std::string result = std::get<std::string>(a.v) + std::get<std::string>(b.v);
                  push(Value::Str(std::move(result)));
                } else if (a.t == Type::Float || b.t == Type::Float) {
                  push(Value::Float(std::get<double>(a.v) + std::get<double>(b.v)));
                }
                else if (a.t == Type::i8 || a.t == Type::i16 || a.t == Type::i32 ||
                         a.t == Type::i64 || a.t == Type::ui8 || a.t == Type::ui16 ||
                         a.t == Type::ui32 ||  a.t == Type::ui64 )
                {
                  // FIXME: Should use largest size
                  push(Value::UI64(std::get<uint64>(a.v) + std::get<uint64>(b.v)));
                }
              } break;
              case static_cast<uint8>(Math::ADD_MULT): {
                uint64 n = GETu64();

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint64 i = 0; i < n; ++i) {
                  operands.push_back(pop());
                }

                // FIXME: Should use largest type given :)
                // Determine result type (use first operand's type)
                Type result_type = operands[0].t;
                Value result;

                switch (result_type) {
                  case Type::i8: {
                    std::vector<int8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int8>(op.v));
                    }
                    result = Value::I8(divide_multi_i8(vals.data(), n));
                  } break;

                  case Type::i16: {
                    std::vector<int16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int16>(op.v));
                    }
                    result = Value::I16(divide_multi_i16(vals.data(), n));
                  } break;

                  case Type::i32: {
                    std::vector<int32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int32>(op.v));
                    }
                    result = Value::I32(divide_multi_i32(vals.data(), n));
                  } break;

                  case Type::i64: {
                    std::vector<int64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int64>(op.v));
                    }
                    result = Value::I64(divide_multi_i64(vals.data(), n));
                  } break;

                  case Type::ui8: {
                    std::vector<uint8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint8>(op.v));
                    }
                    result = Value::UI8(divide_multi_ui8(vals.data(), n));
                  } break;

                  case Type::ui16: {
                    std::vector<uint16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint16>(op.v));
                    }
                    result = Value::UI16(divide_multi_ui16(vals.data(), n));
                  } break;

                  case Type::ui32: {
                    std::vector<uint32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint32>(op.v));
                    }
                    result = Value::UI32(divide_multi_ui32(vals.data(), n));
                  } break;

                  case Type::ui64: {
                    std::vector<uint64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint64>(op.v));
                    }
                    result = Value::UI64(divide_multi_ui64(vals.data(), n));
                  } break;

                  case Type::Float: {
                    std::vector<double> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<double>(op.v));
                    }
                    result = Value::Float(divide_multi_f64(vals.data(), n));
                  } break;

                  default:
                    print("FATAL ERROR: Add values with non-numeric type(s)\n");
                    perr_debug_location_if_available();
                    std::exit(1);
                }

                push(result);
              } break;

              case static_cast<uint8>(Math::DIV_MULT): {
                uint64 n = GETu64();

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint64 i = 0; i < n; ++i) {
                  operands.push_back(pop());
                }
                // FIXME: Should use largest type given :)
                // Determine result type (use first operand's type)
                Type result_type = operands[0].t;
                Value result;

                switch (result_type) {
                  case Type::i8: {
                    std::vector<int8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int8>(op.v));
                    }
                    result = Value::I8(divide_multi_i8(vals.data(), n));
                  } break;

                  case Type::i16: {
                    std::vector<int16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int16>(op.v));
                    }
                    result = Value::I16(divide_multi_i16(vals.data(), n));
                  } break;

                  case Type::i32: {
                    std::vector<int32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int32>(op.v));
                    }
                    result = Value::I32(divide_multi_i32(vals.data(), n));
                  } break;

                  case Type::i64: {
                    std::vector<int64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int64>(op.v));
                    }
                    result = Value::I64(divide_multi_i64(vals.data(), n));
                  } break;

                  case Type::ui8: {
                    std::vector<uint8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint8>(op.v));
                    }
                    result = Value::UI8(divide_multi_ui8(vals.data(), n));
                  } break;

                  case Type::ui16: {
                    std::vector<uint16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint16>(op.v));
                    }
                    result = Value::UI16(divide_multi_ui16(vals.data(), n));
                  } break;

                  case Type::ui32: {
                    std::vector<uint32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint32>(op.v));
                    }
                    result = Value::UI32(divide_multi_ui32(vals.data(), n));
                  } break;

                  case Type::ui64: {
                    std::vector<uint64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint64>(op.v));
                    }
                    result = Value::UI64(divide_multi_ui64(vals.data(), n));
                  } break;

                  case Type::Float: {
                    std::vector<double> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<double>(op.v));
                    }
                    result = Value::Float(divide_multi_f64(vals.data(), n));
                  } break;

                  default:
                    // FIXME: Need better error
                    print("ERROR: Unknown type\n");
                    std::exit(1);
                }

                push(result);
              } break;

              case static_cast<uint8>(Math::SUB_MULT): {
                uint64 n = GETu64();

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint64 i = 0; i < n; ++i) {
                  operands.push_back(pop());
                }
                // FIXME: Should use largest type given :)
                // Determine result type (use first operand's type)
                Type result_type = operands[0].t;
                Value result;

                switch (result_type) {
                  case Type::i8: {
                    std::vector<int8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int8>(op.v));
                    }
                    result = Value::I8(subtract_multi_i8(vals.data(), n));
                  } break;

                  case Type::i16: {
                    std::vector<int16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int16>(op.v));
                    }
                    result = Value::I16(subtract_multi_i16(vals.data(), n));
                  } break;

                  case Type::i32: {
                    std::vector<int32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int32>(op.v));
                    }
                    result = Value::I32(subtract_multi_i32(vals.data(), n));
                  } break;

                  case Type::i64: {
                    std::vector<int64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int64>(op.v));
                    }
                    result = Value::I64(subtract_multi_i64(vals.data(), n));
                  } break;

                  case Type::ui8: {
                    std::vector<uint8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint8>(op.v));
                    }
                    result = Value::UI8(subtract_multi_ui8(vals.data(), n));
                  } break;

                  case Type::ui16: {
                    std::vector<uint16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint16>(op.v));
                    }
                    result = Value::UI16(subtract_multi_ui16(vals.data(), n));
                  } break;

                  case Type::ui32: {
                    std::vector<uint32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint32>(op.v));
                    }
                    result = Value::UI32(subtract_multi_ui32(vals.data(), n));
                  } break;

                  case Type::ui64: {
                    std::vector<uint64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint64>(op.v));
                    }
                    result = Value::UI64(subtract_multi_ui64(vals.data(), n));
                  } break;

                  case Type::Float: {
                    std::vector<double> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<double>(op.v));
                    }
                    result = Value::Float(subtract_multi_f64(vals.data(), n));
                  } break;

                  default:
                    print("FATAL ERROR: Subtracting multiple values with unknown type(s)\n");
                    perr_debug_location_if_available();
                    std::exit(1);
                }

                push(result);
              } break;
              case static_cast<uint8>(Math::MULT_MULT): {
                // FIXME: shorten this, do 64-bit and cast to variables size :)
                // Do this for all others
                uint64 n = GETu64();

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint64 i = 0; i < n; ++i) {
                  operands.push_back(pop());
                }
                // FIXME: Should use largest type given :)
                // Determine result type (use first operand's type)
                Type result_type = operands[0].t;
                Value result;

                switch (result_type) {
                  case Type::i8: {
                    std::vector<int8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int8>(op.v));
                    }
                    result = Value::I8(multiply_multi_i8(vals.data(), n));
                  } break;

                  case Type::i16: {
                    std::vector<int16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int16>(op.v));
                    }
                    result = Value::I16(multiply_multi_i16(vals.data(), n));
                  } break;

                  case Type::i32: {
                    std::vector<int32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int32>(op.v));
                    }
                    result = Value::I32(multiply_multi_i32(vals.data(), n));
                  } break;

                  case Type::i64: {
                    std::vector<int64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<int64>(op.v));
                    }
                    result = Value::I64(multiply_multi_i64(vals.data(), n));
                  } break;

                  case Type::ui8: {
                    std::vector<uint8> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint8>(op.v));
                    }
                    result = Value::UI8(multiply_multi_ui8(vals.data(), n));
                  } break;

                  case Type::ui16: {
                    std::vector<uint16> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint16>(op.v));
                    }
                    result = Value::UI16(multiply_multi_ui16(vals.data(), n));
                  } break;

                  case Type::ui32: {
                    std::vector<uint32> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint32>(op.v));
                    }
                    result = Value::UI32(multiply_multi_ui32(vals.data(), n));
                  } break;

                  case Type::ui64: {
                    std::vector<uint64> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<uint64>(op.v));
                    }
                    result = Value::UI64(multiply_multi_ui64(vals.data(), n));
                  } break;

                  case Type::Float: {
                    std::vector<double> vals;
                    vals.reserve(n);
                    for (const auto& op : operands) {
                      vals.push_back(std::get<double>(op.v));
                    }
                    result = Value::Float(multiply_multi_f64(vals.data(), n));
                  } break;

                  default:
                    print("FATAL ERROR: Multiplying multiple values with unknown type(s)\n");
                    perr_debug_location_if_available();
                    std::exit(1);
                }

                push(result);
              } break;
            } break;
          } break;

          case static_cast<uint8>(Register::VARIABLE): {
            switch (op & 0x1F) {
              case static_cast<uint8>(Variable::PUSH): {
                // Read type byte after opcode (not encoded in opcode due to bit conflicts)
                uint8 typeByte = GETu8();
                switch (typeByte) {
                  case 0x00: {  // Numeric types - meta byte follows
                    unsigned char meta = GETu8();
                    uint8 type = meta >> 4;
                  switch(type) {
                    case 0x00:{
                      push(Value::I8(GETs8()));
                    } break;
                    case 0x01:{
                      push(Value::I16(GETs16()));
                    } break;
                    case 0x02:{
                      push(Value::I32(GETs32()));
                    } break;
                    case 0x03:{
                      push(Value::I64(GETs64()));
                    } break;
                    case 0x04:{
                      push(Value::UI8(GETu8()));
                    } break;
                    case 0x05:{
                      push(Value::UI16(GETu16()));
                    } break;
                    case 0x06:{
                      push(Value::UI32(GETu32()));
                    } break;
                    case 0x07:{
                      push(Value::UI64(GETu64()));
                    } break;
                    case 0x08:{
                      push(Value::Float(GETd64()));
                    } break;
                    case 0x09:{
                      push(Value::Bool(meta & 1));
                    } break;  // Bool - last bit is 0/1
                    case 0x0A:{
                      push(Value::Null());
                    } break;
                    default: {
                      print("FATAL ERROR: Unknown meta tag: ", (int)type, "\n");
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

                case 0x30: {
                  push(Value::Str(std::move(GETstr())));
                } break;  // String
                case 0x40: {  // List
                  uint64 n = GETu64();
                  std::vector<Value> xs; xs.resize(n);
                  for (uint64 i = 0; i < n; ++i) xs[n-1-i] = pop();
                  push(Value::List(std::move(xs)));
                } break;
                /*case 0x50: {  // Dict
                  uint64 n = GETu64();
                  std::map<std::string, Value> dict;
                  for (uint64 i = 0; i < n; ++i) {
                    std::string key = GETstr();
                    Value val = pop();
                    dict[key] = val;
                  }
                  push(Value::Dict(std::move(dict)));
                } break;*/
                default: {
                  print("FATAL ERROR: Unknown literal type tag: 0x", std::hex, (uint32)typeByte, std::dec, "\n");
                  perr_debug_location_if_available();
                  std::exit(1);
                }
              }
            } break;
            case static_cast<uint8>(Variable::SET):{
              frames.back().env->SetOrDeclare(GETstr(), pop());
            } break;

            case static_cast<uint8>(Variable::DECLARE): {
              std::string id = GETstr();
              uint64 tt = GETu64();
              Value v  = pop();
              if (tt == 0xECull) frames.back().env->Declare(id, v);
              else               frames.back().env->Declare(id, v);
            } break;

            case static_cast<uint8>(Variable::GET): {
              std::string id = GETstr();
              push(frames.back().env->Get(id).val);
            } break;

            // FIXME: This is possibly incomplete
            case static_cast<uint8>(Variable::UNSET): {
              std::string id = GETstr();
              if (!frames.back().env->Unset(id)) {}
            } break;
          } break;


          case static_cast<uint8>(Register::GENERAL): {
            switch (op & 0x1F) {
              case static_cast<uint8>(General::HALT): {
                return;
              }
              case static_cast<uint8>(General::NOP): {
                break;
              }
              case static_cast<uint8>(General::POP): discard(); break;
              case static_cast<uint8>(General::YIELD): {
                #ifdef _WIN32
                  _getch();
                #else
                  system("read -n 1");
                #endif
              } break;
              case static_cast<uint8>(General::INDEX): {
                Value base = pop(), idxV = pop();
                uint64 i = std::get<uint64>(idxV.v);
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

          case static_cast<uint8>(Register::FUNCTION): {
            switch (op & 0x1F) {
              // FIXME: Could be simpler to implement via other MVME opcodes
              case static_cast<uint8>(Func::TAIL): {
                std::string name = GETstr();
                uint64 argc = GETu64();
                std::vector<Value> args(argc);
                for (size_t i = 0; i < argc; ++i) args[argc-1-i] = pop();

                auto it = fnEntry.find(name);
                if (it == fnEntry.end()) {
                  auto bit = builtins.find(name);
                  if (bit == builtins.end()) {
                    // FIXME: Should have an error message
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
                  currentFrame.env->Declare(meta.params[i], args[i]);
                }

                jump(meta.entry);
              } break;

              case static_cast<uint8>(Func::RETURN): {
                Value rv;
                if (stack.size() > frames.back().stack_base) {
                  rv = pop();
                } else {
                  // FIXME: DANGEROUS, find some alternative
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
                push(std::move(rv));
              } break;

              case static_cast<uint8>(Func::CALL): {
                // FIXME: We need to add a specific directory for plugin object files!
                std::string name = GETstr();
                uint64 argc = GETu64();
                std::vector<Value> args(argc);
                for (size_t i = 0; i < argc; ++i) args[argc-1-i] = pop();

                auto it = fnEntry.find(name);
                if (it == fnEntry.end()) {
                  auto bit = builtins.find(name);
                  if (bit == builtins.end()) {
                    // Check plugin functions
                    auto pfn = PluginManager::get_function(name);
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
                  frames.back().env->Declare(meta.params[i], args[i]);
                }

                jump(meta.entry);
              } break;
            }
          } break;
          case static_cast<uint8>(Register::IMPORT): {
            // FIXME: We need to find a better way of storage,
            // for the niche case where somehting matches the pieces we have
            // IDEA: We could use a second file?
            /*
            get file with fast_io
            open file
            Read start till "0x193 FEND 0x223" is found
              Formatting is:
              [function name]
              [0x183] [0x212]
              [64-bit location]
              [0x232] [0x139]
              [variable name]
              [0x132] [0x14]
              [64-bit variable location]
            Using this table, we dispatch and run() the found function
            */
            // Simpler way, this simply tells the code, "This is what we have acess to :)" and we base it on file name!
          } break;
          // NOTE: This is going to be given to Rust to do
          case static_cast<uint8>(Register::STACK): {

          } break;
          default: {
            print("FATAL ERROR: Bad or unknown opcode: 0x", std::hex, (uint32)op, std::dec, "\n");
            perr_debug_location_if_available();
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

int main(int argc, char* argv[]){
  run(argv[1]);
}
