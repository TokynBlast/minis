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
#include <array>
#include <conio.h> // Provides Windows _getch()
#include "../include/bytecode.hpp"
#include "../include/types.hpp"
#include "../include/value.hpp"
#include "../include/vm.hpp"
#include "../include/plugin.hpp"
#include "../include/macros.h"
#include "../include/io.hpp"
// #include <fast_io.h>
#include "../fast_io/include/fast_io.h"

#define DEBUGGER true
#define DEBUG_READ_PRINT true
#define DEBUG_BUILTIN_PRINT true
#define DEBUG_OP_PRINT true
#define DEBUG_ALL_PRINT false
#define DEBUG_PROG_CATCH_PRINT false

#if DEBUG_PROG_CATCH_PRINT
#define CATCH_ALL(EXPR) \
  try { \
      EXPR; \
  } catch (const std::bad_variant_access& e) { \
    print("[FATAL] std::bad_variant_access caught!\n"); \
    print("Exception: ", std::string(e.what()), "\n"); \
    print("[DEBUG] VM ip: ", ip, "\n"); \
    print("[DEBUG] Stack size: ", stack.size(), "\n"); \
    print("[DEBUG] Top stack type: ", stack.empty() ? -1 : (int)stack.back().t, "\n"); \
    exit(1); \
  }
#endif

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
      {"print", [](std::vector<Value> &args) {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("Running print\n");
        #endif
        auto print_value = [&](const Value& val, auto&& print_value_ref) -> void {
          switch (val.t) {
            case Type::Float: print(std::get<double>(val.v)); break;
            case Type::Str:   print(std::get<std::string>(val.v)); break;
            case Type::Bool:  print(std::get<bool>(val.v)); break;
            case Type::Null:  print(""); break;
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
            default: print("FARAL EROR: Unknown type "); exit(1);
          }
        };

        size_t arg_amnt = args.size();
        for (size_t i = 0; i < arg_amnt; i++) {
          print_value(args[i], print_value);
          if (i + 1 < arg_amnt) print(" ");
        }
        // FIXME: print should return nothing
        return Value::Null();
      }},
      {"abs", [](std::vector<Value> &args) {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running abs\n");
        #endif
        auto val = args[0];
        if (val.t == Type::Float)
          return Value::Float(std::abs(std::get<double>(val.v)));
        return Value::I64(std::abs(std::get<int64>(val.v)));
      }},
      {"neg", [](std::vector<Value> &args) {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running neg\n");
        #endif
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
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running range\n");
        #endif
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
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running max\n");
        #endif
         Value max = args[0];
         for (size_t i = 1; i < args.size(); i++)
         {
           if (args[i] > max)
             max = args[i];
         }
         return max;
       }},
      {"min", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running min\n");
        #endif
        Value min = args[0];
        for (size_t i = 1; i < args.size(); i++) {
          if (args[i] < min)
            min = args[i];
        }
        return min;
      }},
      {"sort", [](std::vector<Value> &args) {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running sort\n");
        #endif
         std::vector<Value> list = std::get<std::vector<Value>>(args[0].v);
         std::sort(list.begin(), list.end(),
                   [](const Value &a, const Value &b)
                   { return std::get<double>(a.v) < std::get<double>(b.v); });
         return Value::List(list);
       }},
      {"reverse", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running reverse\n");
        #endif
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
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running sum\n");
        #endif
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
        #if DEBUGGER
          print("getting input\n");
        #endif
        std::string input;
        if (!args.empty()) {
          print(""); // print the values in here :)
                     // maybe run built-in print function?
        }
        scan(input);
        return Value::Str(std::move(input));
      }},
      {"len", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running len\n");
        #endif
        const auto &arg = args[0];
        if (arg.t == Type::List) {
          return Value::UI64(static_cast<uint64>(std::get<std::vector<Value>>(arg.v).size()));
        } else if (arg.t == Type::Str) {
          return Value::UI64(static_cast<uint64>(std::get<std::string>(arg.v).length()));
        }
        return Value::Null();
       }},
      {"split", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running split\n");
        #endif
        const std::string &str = std::get<std::string>(args[0].v);
        const std::string &delim = std::get<std::string>(args[1].v);
        #if DEBUGGER and DEBUG_BUILTIN_PRINT
          print("splitting by ", delim, "\n");
        #endif

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
      {"upper", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running upper\n");
        #endif
        // >> 30 :)
        std::string result = std::get<std::string>(args[0].v);
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return Value::Str(std::move(result));
      }},
      {"lower", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running lower\n");
        #endif
        std::string result = std::get<std::string>(args[0].v);
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return Value::Str(std::move(result));
      }},
      {"round", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running round\n");
        #endif
        return Value::I64((int64)std::round(std::get<double>(args[0].v)));
      }},
      // Implement via Fortran and C++
      {"random", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running random\n");
        #endif
        // use random lib instead
        if (args.empty()) {
          return Value::Float((double)rand() / RAND_MAX);
        } else {
          uint64 max_val = std::get<uint64>(args[0].v);
          return Value::UI64(rand() % max_val);
        }
      }},

      // FIXME: Use fast_io :)
      {"open", [](std::vector<Value> &args) -> Value {
        const char *filename = std::get<std::string>(args[0].v).c_str();
        const char *mode_str = std::get<std::string>(args[1].v).c_str();
        // FIXME: This should be a switch instead
        // Translate Minis modes to C modes
        const char *c_mode = "r";
        // FIXME: Simplify to normality
        if (strcmp(mode_str, "r") == 0) c_mode = "r";
        else if (strcmp(mode_str, "rb") == 0) c_mode = "rb";
        else if (strcmp(mode_str, "rs") == 0) c_mode = "r"; // read specific = read but read only a section, rather than put the whole file into mem
        else if (strcmp(mode_str, "w") == 0) c_mode = "w";
        else if (strcmp(mode_str, "wb") == 0) c_mode = "wb";
        else if (strcmp(mode_str, "ws") == 0) c_mode = "w"; // write specific = write but write to a specific area rather than rewrite the whole thing
        /*write targeted = append but write anywhere, not just the end, shifting alldata after up
         * For example:
         * appending ", " to five (5) in "Helloworld" makes it "Hello, world"
         */
        else if (strcmp(mode_str, "wt") == 0) c_mode = "a";
        else if (strcmp(mode_str, "a") == 0) c_mode = "a"; // also support append directly

        FILE *file = fopen(filename, c_mode);
        if (!file) return Value::Bool(1); // Error code

        // Store file pointer as integer (simple approach)
        return Value::UI64((uint64)(uintptr_t)file);
       }},

      {"close", [](std::vector<Value> &args) -> Value {
        FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
        if (file && file != stdin && file != stdout && file != stderr)
        {
          fclose(file);
        }
         return Value::Bool(false);
       }},

      {"write", [](std::vector<Value> &args) -> Value {
         FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
         const char *data = std::get<std::string>(args[1].v).c_str();

         if (!file) std::exit(1);

         size_t written = fwrite(data, 1, strlen(data), file);
         return Value::Bool((bool)written);
       }},

      {"read", [](std::vector<Value> &args) -> Value {
        try {
          std::string filename = std::get<std::string>(args[0].v);
          fast_io::native_file_loader loader(filename);

          #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
            std::string val = std::string(loader.data(), loader.size());
            print("opening ", filename, "\n");
            Value result = Value::Str(std::move(val));
          #else
            Value result = Value::Str(std::string(loader.data(), loader.size()));
          #endif
          return result;
        } catch (const std::exception& e) {
          print("File I/O error: ", std::string(e.what()), "\n");
          exit(1);
        }
       }},

      // FIXME: We don't need to take any arguments
      {"flush", [](std::vector<Value> &args) -> Value {
         FILE *file = (FILE *)(uintptr_t)std::get<int32>(args[0].v);
         if (file)
         {
           fflush(file);
         }
         return Value::UI8(0);
       }},

      // FIXME: We need better type checking
      // FIXME: Add returning multiple values
      {"typeof", [](std::vector<Value> &args) -> Value {
        #if DEBUGGER and DEBUG_BUILTIN_PRINT or DEBUG_ALL_PRINT and DEBUGGER
          print("running typeof\n");
        #endif
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
      Type ret;
      std::vector<std::string> params;
    };
    std::unordered_map<std::string, FnMeta> fnEntry;

    explicit VMEngine() {}
    ~VMEngine() { if (f) fclose(f); }

    inline void jump(uint64 target) {
      #if DEBUGGER
        print("Jumping to ", target, "\n");
      #endif
      ip = target; fseek(f, (long)ip, SEEK_SET);
    }

    // unsigned ints
    inline uint8 GETu8() {
      #if DEBUGGER
        print("Get ui8\n");
      #endif
      uint8 v;
      fread(&v, 1, 1, f);
      ip++;
      return v;
    }

    inline uint16 GETu16() {
      #if DEBUGGER
        print("Get ui16\n");
      #endif
      uint16 v;
      fread(&v, 2, 1, f);
      ip+=2;
      return v;
    }
    inline uint32 GETu32() {
      #if DEBUGGER
        print("Get ui32\n");
      #endif
      uint32 v;
      fread(&v, 4, 1, f);
      ip+=4;
      return v;
    }

    inline uint64 GETu64(
      #if DEBUGGER
        bool from_GETstr = false
      #endif
    ) {
      #if DEBUGGER
        if (!from_GETstr) print("Get ui64\n");
      #endif
      uint64 v;
      fread(&v, 8, 1, f);
      ip+=8;
      return v;
    }

    // signed ints
    inline int8 GETs8() {
      #if DEBUGGER
        print("Get i8\n");
      #endif
      int8 v;
      fread(&v, 1, 1, f);
      ip++;
      return v;
    }

    inline int8 GETs16() {
      #if DEBUGGER
        print("Get i16\n");
      #endif
      int16 v;
      fread(&v, 2, 1, f);
      ip+=2;
      return v;
    }

    inline int8 GETs32() {
      #if DEBUGGER
        print("Get i32\n");
      #endif
      int32 v;
      fread(&v, 4, 1, f);
      ip+=4;
      return v;
    }

    inline int64 GETs64() {
      #if DEBUGGER
        print("Get i64\n");
      #endif
      int64 v;
      fread(&v, 8, 1, f);
      ip += 8;
      return v;
    }

    inline double GETd64() {
      #if DEBUGGER
        print("get double\n");
      #endif
      double v;
      fread(&v, 8, 1, f);
      ip += 8;
      return v;
    }

    inline std::string GETstr() {
      // FIXME: simplify debugger version
      uint64 n=GETu64(
        #if DEBUGGER
          true // Mark as coming from string, so don't print we got the ui64
        #endif
      );
      #if DEBUGGER
        print("Get ", n, " character string, \"");
      #endif
      if(n == 0) {
        #if DEBUGGER
          print("\"\n");
        #endif
        return std::string();
      }

      std::string s(n, '\0');
      fread(&s[0], 1, n, f);
      ip += n;
      #if DEBUGGER
        print(s, "\"\n");
      #endif
      return s;
    }

    inline Value pop() {
      #if DEBUGGER
        print("Popping value\n");
      #endif
      try {
        if (stack.empty()) {
          print("FATAL ERROR: Stack underflow. Tried to pop an empty stack.");
          std::exit(1);
        }
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
      if (std::memcmp(magic, "  \xc2\xbd" "6e" "\xc3\xa8", 8) != 0)  // "  ½6eè" = 8 bytes
        throw std::runtime_error("bad bytecode verification");

      table_off = GETu64();
      uint64 fnCount = GETu64();
      uint64 entry_main = GETu64();
      uint64 line_map_off = GETu64();

      // Header is now exactly 40 bytes (8 magic + 32 data), no padding
      ip = entry_main;
      code_end = table_off;

      fseek(f, (long)table_off, SEEK_SET);
      for (uint64 i = 0; i < fnCount; i++) {
        std::string name = GETstr();
        uint64 entry = GETu64();
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
        switch (op >> 5) {
          case static_cast<uint8>(Register::LOGIC): {
            switch (op & 0x1F) {
              case static_cast<uint8>(Logic::EQUAL): {
                Value a = pop(), b = pop();
                bool eq = (a.t == b.t) ? (a == b)
                          : ((a.t != Type::Str && a.t != Type::List && b.t != Type::Str && b.t != Type::List)
                              ? (std::get<double>(a.v) == std::get<double>(b.v)) : false);
                push(Value::Bool(eq));
              } break;

              case static_cast<uint8>(Logic::JUMP): {
                #if DEBUGGER
                  uint64 tgt = GETu64();
                  print("jumping to ", tgt, "\n");
                  jump(tgt);
                #else
                  jump(GETu64());
                #endif
              } break;
              case static_cast<uint8>(Logic::JUMP_IF): {
                uint64 tgt = GETu64();
                Value v = pop();
                if (std::get<bool>(v.v)) {
                  #if DEBUGGER
                    print("jumping to ", tgt, "\n");
                  #endif
                  jump(tgt);
                }
              } break;
              case static_cast<uint8>(Logic::JUMP_IF_NOT): {
                uint64 tgt = GETu64();
                Value v = pop();
                if (!std::get<bool>(v.v)) {
                  #if DEBUGGER
                    print("jumping to ", tgt, "\n");
                  #endif
                  jump(tgt);
                }
                #if DEBUGGER
                  print("did not jump to ", tgt, "\n");
                #endif
              } break;
              case static_cast<uint8>(Logic::AND): {
                #if DEBUGGER
                  print("and op\n");
                #endif
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
                #if DEBUGGER
                    print("or op\n");
                  #endif
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
                if (a.t == Type::Str && b.t == Type::Str)
                  push(Value::Bool(std::get<std::string>(a.v) <= std::get<std::string>(b.v)));
                else if (a.t == Type::Float || b.t == Type::Float)
                  push(Value::Bool(std::get<double>(a.v) <= std::get<double>(b.v)));
                else if (a.t == Type::ui64 || b.t == Type::ui64)
                  push(Value::Bool(std::get<uint64>(a.v) <= std::get<uint64>(b.v)));
                else if (a.t == Type::i64 || b.t == Type::i64)
                  push(Value::Bool(std::get<int64>(a.v) <= std::get<int64>(b.v)));
                else if (a.t == Type::ui32 || b.t == Type::ui32)
                  push(Value::Bool(std::get<uint32>(a.v) <= std::get<uint32>(b.v)));
                else if (a.t == Type::i32 || b.t == Type::i32)
                  push(Value::Bool(std::get<int32>(a.v) <= std::get<int32>(b.v)));
                else
                  push(Value::Bool(std::get<double>(a.v) <= std::get<double>(b.v)));
              } break;

              case static_cast<uint8>(Logic::LESS_THAN): {
                Value a = pop(), b = pop();
                if (a.t == Type::Str && b.t == Type::Str)
                  push(Value::Bool(std::get<std::string>(a.v) < std::get<std::string>(b.v)));
                else if (a.t == Type::Float || b.t == Type::Float)
                  push(Value::Bool(std::get<double>(a.v) < std::get<double>(b.v)));
                else if (a.t == Type::ui64 || b.t == Type::ui64)
                  push(Value::Bool(std::get<uint64>(a.v) < std::get<uint64>(b.v)));
                else if (a.t == Type::i64 || b.t == Type::i64)
                  push(Value::Bool(std::get<int64>(a.v) < std::get<int64>(b.v)));
                else if (a.t == Type::ui32 || b.t == Type::ui32)
                  push(Value::Bool(std::get<uint32>(a.v) < std::get<uint32>(b.v)));
                else if (a.t == Type::i32 || b.t == Type::i32)
                  push(Value::Bool(std::get<int32>(a.v) < std::get<int32>(b.v)));
                else
                  push(Value::Bool(std::get<double>(a.v) < std::get<double>(b.v)));
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
              case static_cast<uint8>(Math::SUB): {
                #if DEBUGGER
                  print("Subtracting\n");
                #endif
                Value a = pop(), b = pop();
                if ((a.t == Type::Int || a.t == Type::Float) && (b.t == Type::Int || b.t == Type::Float)) {
                  if (a.t == Type::Float || b.t == Type::Float) {
                    push(Value::Float(std::get<double>(a.v) - std::get<double>(b.v)));
                  } else {
                    push(Value::I64(std::get<int64>(a.v) - std::get<int64>(b.v)));
                  }
                }
              } break;
              case static_cast<uint8>(Math::MULT): {
                #if DEBUGGER
                  // FIXME: This should print out the values to be multiplied :)
                  print("multiplying\n");
                #endif
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
                #if DEBUGGER
                  print("add op\n");
                #endif
                Value a = pop(), b = pop();
                // FIXME: This needs to be tested for bugs heavily; When it was found for sorting,
                //        I found the first if as an else if.
                #if DEBUGGER
                  print("[ADD] a type: ", (int)a.t, ", b type: ", (int)b.t, "\n");
                  // Print value types for a and b
                  switch (a.t) {
                    case Type::List: print("a is list\n"); break;
                    case Type::Str: print("a is str: ", std::get<std::string>(a.v), "\n"); break;
                    case Type::Float: print("a is float: ", std::get<double>(a.v), "\n"); break;
                    case Type::i64: print("a is i64: ", std::get<int64>(a.v), "\n"); break;
                    case Type::ui64: print("a is ui64: ", std::get<uint64>(a.v), "\n"); break;
                    default: print("a is other type\n"); break;
                  }
                  switch (b.t) {
                    case Type::List: print("b is list\n"); break;
                    case Type::Str: print("b is str: ", std::get<std::string>(b.v), "\n"); break;
                    case Type::Float: print("b is float: ", std::get<double>(b.v), "\n"); break;
                    case Type::i64: print("b is i64: ", std::get<int64>(b.v), "\n"); break;
                    case Type::ui64: print("b is ui64: ", std::get<uint64>(b.v), "\n"); break;
                    default: print("b is other type\n"); break;
                  }
                #endif
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
                uint32 n = GETu32();

                if (n == 0) {
                  push(Value::I32(0));
                  break;
                }

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint32 i = 0; i < n; ++i) {
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
                    // FIXME: Need better error message
                    print("ERROR: ADD_MULTI called with non-numeric type\n");
                    std::exit(1);
                }

                push(result);
              } break;

              case static_cast<uint8>(Math::DIV_MULT): {
                uint32 n = GETu32();

                #if DEBUGGER
                  print("div ", n, " values\n");
                #endif

                if (n == 0) {
                  push(Value::I32(0));
                  break;
                }

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint32 i = 0; i < n; ++i) {
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
                uint32 n = GETu32();

                if (n == 0) {
                  push(Value::I32(0));
                  break;
                }

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint32 i = 0; i < n; ++i) {
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
                    // FIXME: Need better error
                    print("ERROR: Unknown type\n");
                    std::exit(1);
                }

                push(result);
              } break;
              case static_cast<uint8>(Math::MULT_MULT): {
                // FIXME: shorten this, do 64-bit and cast to variables size :)
                // Do this for all others
                uint32 n = GETu32();

                if (n == 0) {
                  push(Value::I32(0));
                  break;
                }

                // Collect operands from stack
                std::vector<Value> operands;
                operands.reserve(n);
                for (uint32 i = 0; i < n; ++i) {
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
                    // FIXME: Need better error
                    print("ERROR: Unknown type\n");
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
                      #if DEBUGGER
                        print("Push i8\n");
                      #endif
                      push(Value::I8(GETs8()));
                    } break;
                    case 0x01:{
                      #if DEBUGGER
                        print("Push i16\n");
                      #endif
                      push(Value::I16(GETs16()));
                    } break;
                    case 0x02:{
                      #if DEBUGGER
                        print("Push i32\n");
                      #endif
                      push(Value::I32(GETs32()));
                    } break;
                    case 0x03:{
                      #if DEBUGGER
                        print("Push i64\n");
                      #endif
                      push(Value::I64(GETs64()));
                    } break;
                    case 0x04:{
                      #if DEBUGGER
                        print("Push ui8\n");
                      #endif
                      push(Value::UI8(GETu8()));
                    } break;
                    case 0x05:{
                      #if DEBUGGER
                        print("Push ui16\n");
                      #endif
                      push(Value::UI16(GETu16()));
                    } break;
                    case 0x06:{
                      #if DEBUGGER
                        print("Push ui32\n");
                      #endif
                      push(Value::UI32(GETu32()));
                    } break;
                    case 0x07:{
                      #if DEBUGGER
                        print("Push ui64\n");
                      #endif
                      push(Value::UI64(GETu64()));
                    } break;
                    case 0x08:{
                      #if DEBUGGER
                        print("Push double\n");
                      #endif
                      push(Value::Float(GETd64()));
                    } break;
                    case 0x09:{
                      #if DEBUGGER
                        print("Push bool\n");
                      #endif
                      push(Value::Bool(meta & 1));
                    } break;  // Bool - last bit is 0/1
                    case 0x0A:{
                      #if DEBUGGER
                        print("Push null\n");
                      #endif
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
                  #if DEBUGGER
                    std::string val = GETstr();
                    print("Push string ", val, "\n");
                    push(Value::Str(std::move(val)));
                  #else
                    push(Value::Str(std::move(GETstr())));
                  #endif
                } break;  // String
                case 0x40: {  // List
                  uint64 n = GETu64();
                  std::vector<Value> xs; xs.resize(n);
                  for (uint64 i = 0; i < n; ++i) xs[n-1-i] = pop();
                  #if DEBUGGER
                    print("push ", n, " item list: [");
                    for (uint64 i = 0; i < n; ++i) {
                      print_value(xs[i]);
                      if (i + 1 < n) print(" ");
                    }
                    print("]\n");
                  #endif
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
                  print("unknown literal type tag: ", (int)typeByte, "\n");
                  std::exit(1);
                }
              }
            } break;
            case static_cast<uint8>(Variable::SET):{
              #if DEBUGGER
                std::string str = GETstr();
                print("setting ", str, "\n");
                frames.back().env->SetOrDeclare(std::move(str), pop());
              #else
                frames.back().env->SetOrDeclare(GETstr(), pop());
              #endif
            } break;

            case static_cast<uint8>(Variable::DECLARE): {
              std::string id = GETstr();
              uint64 tt = GETu64();
              #if DEBUGGER
                print("declaring variable ", id, "\n");
              #endif
              Value v  = pop();
              if (tt == 0xECull) frames.back().env->Declare(id, v);
              else               frames.back().env->Declare(id, v);
            } break;

            case static_cast<uint8>(Variable::GET): {
              std::string id = GETstr();
              #if DEBUGGER
                print("getting ", id, "\n");
              #endif
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
                #if DEBUGGER
                  print("halting\n");
                #endif
                return;
              }
              case static_cast<uint8>(General::NOP): {
                #if DEBUGGER
                  print("doing nothing\n");
                #endif
                break;
              }
              case static_cast<uint8>(General::POP): discard(); break;
              case static_cast<uint8>(General::YIELD): {
                #if DEBUGGER
                  print("yielding\n");
                #endif

                #if _WIN32
                  _getch();
                #else
                  system("read -n 1");
                #endif
              } break;
              case static_cast<uint8>(General::INDEX): {
                Value base = pop(), idxV = pop();
                long long i = std::get<int32>(idxV.v);
                #if DEBUGGER
                  print("indexing at ", i, "\n");
                #endif
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
                #if DEBUGGER
                  print("tail call return\n");
                #endif
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
                #if DEBUGGER
                  print("returning value\n");
                #endif
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

              case static_cast<uint8>(Func::CALL): {
                std::string name = GETstr();
                uint64 argc = GETu64();
                #if DEBUGGER
                  print("calling ", name, " with ", argc, " arguments\n");
                #endif
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
          } break;
          case static_cast<uint8>(Register::STACK): {

          } break;
          default: {
            print("FATAL ERROR: Bad or unknown opcode. ", op);
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
    #if DEBUGGER
      print("Loading VM\n");
    #endif
    vm.load(path);
    #if DEBUGGER
      print("Running\n");
    #endif
    #if DEBUGGER and DEBUG_PROG_CATCH_PRINT
      CATCH_ALL(vm.run());
    #else
      vm.run();
    #endif
    #if DEBUGGER
      print("done\n");
    #endif
  }
}

int main(int argc, char* argv[]){
  run(argv[1]);
}
