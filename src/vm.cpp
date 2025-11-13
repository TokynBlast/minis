#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "../include/bytecode.hpp"
#include "../include/types.hpp"
#include "../include/io.hpp"
#include "../include/err.hpp"
#include "../include/value.hpp"
#include "../include/vm.hpp"
#include "../include/ast.hpp"
#include "../include/sso.hpp"

namespace lang {
  /*=============================
  =          Values/Env         =
  =============================*/

  struct Pos{ size_t i=0; const CString* src=nullptr; };
  inline const Source* src = nullptr;

  // helper: build a Loc from the current global Source pointer and a raw index
  static inline Loc locate(size_t index) {
    if (src) return src->loc(index);
    Loc L; L.line = 1; L.col = 1; L.src = "";
    return L;
  }

  // Built-in function handler signature
  using BuiltinFn = std::function<Value(std::vector<Value>&)>;

  // Registry of built-in functions
  inline static Pos p;
  inline static std::unordered_map<CString, BuiltinFn> builtins = {
    {"print", [](std::vector<Value>& args) {
      for (const auto& arg : args) std::cout << arg.AsStr() << " ";
      std::cout << std::endl;
      return Value::N();
    }},
    {"abs", [](std::vector<Value>& args){
      if (args.size() != 1) {
        Loc L = locate(p.i);
        ERR(L, "abs requires exactly one argument");
      }
      auto val = args[0];
      if (val.t == Type::Float) return Value::F(std::abs(val.AsFloat(p.i)));
      return Value::I(std::abs(val.AsInt(p.i)));
    }},
    {"neg", [](std::vector<Value>& args) {
      if (args.size() != 1) {
        Loc L = locate(p.i);
        ERR(L, "neg requires exactly one argument");
      }
      auto val = args[0];
      if (val.t == Type::Float) return Value::F(-val.AsFloat(p.i));
      return Value::I(-val.AsInt(p.i));
    }},
    {"range", [](std::vector<Value>& args){
      if (args.empty() || args.size() > 2) {
        Loc L = locate(p.i);
        ERR(L, "range expects 1-2 arguments");
      }

      long long start = 0, end;
      if (args.size() == 1) {
        end = args[0].AsInt(p.i);
      } else {
        start = args[0].AsInt(p.i);
        end = args[1].AsInt(p.i);
      }

      std::vector<Value> result;
      for (long long i = start; i <= end; i++) {
        result.push_back(Value::I(i));
      }
      return Value::L(result);
    }},
    {"max", [](std::vector<Value>& args) {
      if (args.empty()) {
        Loc L = locate(p.i);
        ERR(L, "max requires at least one argument");
      }
      Value max = args[0];
      for (size_t i = 1; i < args.size(); i++) {
        if (args[i].AsFloat(p.i) > max.AsFloat(p.i)) max = args[i];
      }
      return max;
    }},
    {"min", [](std::vector<Value>& args) {
      if (args.empty()) {
        Loc L = locate(p.i);
        ERR(L, "min requires at least one argument");
      }
      Value min = args[0];
      for (size_t i = 1; i < args.size(); i++) {
        if (args[i].AsFloat(p.i) < min.AsFloat(p.i)) min = args[i];
      }
      return min;
    }},
    {"sort", [](std::vector<Value>& args) {
      if (args.size() != 1 || args[0].t != Type::List) {
        Loc L = locate(p.i);
        ERR(L, "sort requires one list argument");
      }
      std::vector<Value> list = args[0].AsList();
      std::sort(list.begin(), list.end(),
        [](const Value& a, const Value& b) { return a.AsFloat(0) < b.AsFloat(0); });
      return Value::L(list);
    }},
    {"reverse", [](std::vector<Value>& args) {
      if (args.size() != 1) {
        Loc L = locate(p.i);
        ERR(L, "reverse requires one argument");
      }
      if (args[0].t == Type::List) {
        std::vector<Value> list = args[0].AsList();
        std::reverse(list.begin(), list.end());
        return Value::L(list);
      } else if (args[0].t == Type::Str) {
        const char* str = args[0].AsStr();
        CString reversed(str);
        // Manual reverse since CString doesn't have iterators
        size_t len = reversed.size();
        for (size_t i = 0; i < len / 2; ++i) {
          char temp = reversed[i];
          reversed[i] = reversed[len - 1 - i];
          reversed[len - 1 - i] = temp;
        }
        return Value::S(std::move(reversed));
      }
      {
        Loc L = locate(p.i);
        ERR(L, "reverse requires list or string argument");
      }
    }},
    {"sum", [](std::vector<Value>& args) {
      if (args.size() != 1 || args[0].t != Type::List) {
        Loc L = locate(p.i);
        ERR(L, "sum requires one list argument");
      }
      const auto& list = args[0].AsList();
      Value sum = Value::I(0);
      for (const auto& v : list) {
        if (v.t == Type::Float) sum = Value::F(sum.AsFloat(p.i) + v.AsFloat(p.i));
        else sum = Value::I(sum.AsInt(p.i) + v.AsInt(p.i));
      }
      return sum;
    }},
    {"input", [](std::vector<Value>& args) {
      if (!args.empty()) {
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
    {"len", [](std::vector<Value>& args) {
      if (args.size() != 1) {
        Loc L = locate(p.i);
        ERR(L, "len requires exactly one argument");
      }
      const auto& arg = args[0];
      if (arg.t == Type::List) return Value::I(std::get<std::vector<Value>>(arg.v).size());
      else if (arg.t == Type::Str) return Value::I(std::strlen(arg.AsStr()));
      {
        Loc L = locate(p.i);
        ERR(L, "len requires a list or string");
      }
    }},
  };

  void Coerce(Type t, Value& v){
    if (v.t == t) return;
    switch (t){
      case Type::Int:   v = Value::I(v.AsInt(p.i)); break;
      case Type::Float: v = Value::F(v.AsFloat(p.i)); break;
      case Type::Bool:  v = Value::B(v.AsBool(p.i)); break;
      case Type::List:  v = Value::L(v.AsList()); break;
      case Type::Str:   v = Value::S(v.AsStr()); break;
      case Type::Null:  v = Value::N(); break;
    }
  }

  struct Env{
    struct Var{
      Type declared;
      Value val;
      Var(Type t, Value v) : declared(t), val(v) {}
    };

    std::unordered_map<CString,Var> m;
    Env* parent=nullptr;
    Value val;

    explicit Env(Env*p=nullptr) : parent(p), val{} {}

    bool ExistsLocal(const CString&n)const{ return m.find(n)!=m.end();}

    bool Exists(const CString&n)const{ return ExistsLocal(n)||(parent && parent->Exists(n));}

    const Var& Get(const CString&n, auto loc)const{
      auto it=m.find(n);
      if(it!=m.end()) return it->second;
      if(parent) return parent->Get(n,loc);
      {
        Loc L = locate((size_t)loc);
        CString msg = CString("unknown variable '") + n.c_str() + "'";
        ERR(L, msg.c_str());
      }
    }

    void Declare(const CString&n,Type t,Value v, auto loc){
      if(m.count(n)) {
        Loc L = locate((size_t)loc);
        ERR(L, "variable already declared");
      }
      Coerce(t,v); m.emplace(n,Var{t,v});
    }

    void Set(const CString&n,Value v, auto loc){
      auto it=m.find(n); if(it!=m.end()){ Coerce(it->second.declared,v); it->second.val=v; return; }
      if(parent){ parent->Set(n,v,loc); return; }
      {
        Loc L = locate((size_t)loc);
        ERR(L, "unknown variable");
      }
    }

    void SetOrDeclare(const CString&n,Value v, auto loc){ if(ExistsLocal(n)) Set(n,v,loc); else if(parent&&parent->Exists(n)) parent->Set(n,v,loc); else Declare(n,v.t,v,loc); }

    bool Erase(const CString&n){ return m.erase(n)!=0; }

    bool Unset(const CString& n){
      if (Erase(n)) return true;
      return parent ? parent->Unset(n) : false;
    }

  };

  struct VMEngine {
    Env globals;

    FILE* f=nullptr; uint64_t ip=0, table_off=0, code_end=0;
    std::vector<Value> stack;

    struct Frame{
      uint64_t ret_ip;
      std::unique_ptr<Env> env;   // heap-allocated, stable address
      bool isVoid = false;
      bool typed  = false;
      Type ret    = Type::Int;

      Frame(uint64_t rip, std::unique_ptr<Env> e, bool v, bool t, Type r)
        : ret_ip(rip), env(std::move(e)), isVoid(v), typed(t), ret(r) {}
    };

    std::vector<Frame> frames;

    struct FnMeta{ uint64_t entry; bool isVoid; bool typed; Type ret; std::vector<CString> params; };
    std::unordered_map<CString, FnMeta> fnEntry;

    explicit VMEngine() {}

      inline void jump(uint64_t target){ ip = target; fseek(f,(long)ip,SEEK_SET); }
      inline uint8_t  fetch8(){ uint8_t v; fread(&v,1,1,f); ++ip; return v; }
      inline uint64_t fetch64(){ uint64_t v; fread(&v,8,1,f); ip+=8; return v; }
      inline int64_t  fetchs64(){ int64_t v; fread(&v,8,1,f); ip+=8; return v; }
      inline double   fetchf64(){ double v; fread(&v,8,1,f); ip+=8; return v; }
      inline CString fetchStr(){
        uint64_t n=fetch64();
        char* buffer = static_cast<char*>(malloc(n + 1));
        if(n) fread(buffer, 1, n, f);
        buffer[n] = '\0';
        CString result(buffer);
        free(buffer);
        ip += n;
        return result;
      }

      inline Value pop(){
        try {
          if(stack.empty()) {
            Loc L = locate(p.i);
            ERR(L, "stack underflow");
          }
          if(stack.back().t == Type::Null) {
            Loc L = locate(p.i);
            ERR(L, "attempt to use null value");
          }
          Value v=std::move(stack.back());
          stack.pop_back();
          return v;
        } catch(const std::exception& e) {
          Loc L = locate(p.i);
          CString msg = CString("stack operation failed: ") + e.what();
          ERR(L, msg.c_str());
        }
      }
      inline void push(Value v){ stack.push_back(std::move(v)); }
      inline void discard() {
        if (stack.empty()) {
          Loc L = locate((size_t)ip);
          ERR(L, "stack underflow");
        }
        stack.pop_back();
      }

      inline void load(const CString& path){
        f=fopen(path.c_str(),"rb"); if(!f) throw std::runtime_error("cannot open bytecode");
        char magic[9]={0}; fread(magic,1,8,f); if(std::strcmp(magic, "AVOCADO1") != 0) throw std::runtime_error("bad bytecode verification");
        table_off = read_u64(f); uint64_t fnCount = read_u64(f); uint64_t entry_main = read_u64(f);
        ip = entry_main; code_end = table_off;

        fseek(f, (long)table_off, SEEK_SET);
        for(uint64_t i=0;i<fnCount;++i){
          CString name     = read_str(f);
          uint64_t entry   = read_u64(f);
          bool isVoid      = read_u8(f)!=0;
          bool typed       = read_u8(f)!=0;
          Type ret         = (Type)read_u8(f);
          uint64_t pcnt    = read_u64(f);
          std::vector<CString> params; params.reserve((size_t)pcnt);
          for(uint64_t j=0;j<pcnt;++j) params.push_back(read_str(f));
          fnEntry[name] = FnMeta{ entry, isVoid, typed, ret, params };
        }
        jump(entry_main);
        frames.push_back(Frame{ (uint64_t)-1, nullptr, true, false, Type::Int });
        frames.back().env = std::make_unique<Env>(&globals);

      }

    inline void run(){
      for(;;){
        if (ip >= code_end) return;
        uint64_t op = fetch64();

        switch (op) {
          case HALT: return;
          case NOP:  break;

          case PUSH_I: { push(Value::I(fetchs64())); } break;
          case PUSH_F: { push(Value::F(fetchf64())); } break;
          case PUSH_B: { push(Value::B(fetch8()!=0)); } break;
          case PUSH_S: { auto s = fetchStr(); push(Value::S(std::move(s))); } break;

          case MAKE_LIST: {
            uint64_t n = fetch64();
            std::vector<Value> xs; xs.resize(n);
            for (uint64_t i=0; i<n; ++i) xs[n-1-i] = pop();
            push(Value::L(std::move(xs)));
          } break;

          case GET: {
            auto id = fetchStr();
            push(frames.back().env->Get(id, ip).val);
          } break;

          case SET: {
            auto id = fetchStr();
            auto v  = pop();
            frames.back().env->Env::SetOrDeclare(id,v,0);
          } break;

          case DECL: {
            auto id = fetchStr();
            uint64_t tt = fetch64();
            auto v  = pop();
            if (tt == 0xECull) frames.back().env->Env::Declare(id,v.t,v,0);
            else            frames.back().env->Env::Declare(id,(Type)tt,v,0);
          } break;

          case POP: { discard(); } break;

          case ADD: {
            auto b = pop();
            auto a = pop();

            // Handle addition based on type combinations
            if (a.t == Type::Null || b.t == Type::Null) {
              Loc L = locate(p.i);
              ERR(L, "Cannot perform addition with null values");
            }
            else if (a.t == Type::List) {
              if (b.t == Type::List) {
                // More efficient: Reserve space for concatenation
                std::vector<Value> result;
                const auto& L = std::get<std::vector<Value>>(a.v);
                const auto& R = std::get<std::vector<Value>>(b.v);
                result.reserve(L.size() + R.size());
                result.insert(result.end(), L.begin(), L.end());
                result.insert(result.end(), R.begin(), R.end());
                push(Value::L(std::move(result)));  // Move semantics for efficiency
              } else {
                std::vector<Value> result = std::get<std::vector<Value>>(a.v);
                result.push_back(b);
                push(Value::L(std::move(result)));
              }
            } else if (a.t == Type::Str || b.t == Type::Str) {
              CString result = CString(a.AsStr()) + b.AsStr();
              push(Value::S(std::move(result)));
            } else if (a.t == Type::Float || b.t == Type::Float) {
              push(Value::F(a.AsFloat(p.i) + b.AsFloat(p.i))); // Any numeric with Float = Float
            } else if (a.t == Type::Int || b.t == Type::Int) {
              push(Value::I(a.AsInt(p.i) + b.AsInt(p.i))); // Int or Bool = Int
            }else if (a.t == Type::Float && b.t == Type::Int || a.t == Type::Int && b.t == Type::Float){
              push(Value::F(a.AsFloat(p.i) + b.AsFloat(p.i)));
            } else {
              Loc L = locate(p.i);
              CString msg = CString("Cannot add values of type ") + TypeName(a.t) + " and " + TypeName(b.t);
              ERR(L, msg.c_str());
            }
          } break;

          case UNSET: {
          auto id = fetchStr();
            if (!frames.back().env->Unset(id)) {
              Loc L = locate(p.i);
              ERR(L, "unknown variable");
            }
          } break;

          case TAIL: {
            auto name = fetchStr();
            uint64_t argc = fetch64();
            std::vector<Value> args(argc);
            for (size_t i=0; i<argc; ++i) args[argc-1-i] = pop();

            auto it = fnEntry.find(name);
            if (it == fnEntry.end()) {
              auto bit = builtins.find(name);
              if (bit == builtins.end()) {
                Loc L = locate(p.i);
                ERR(L, "unknown function");
              }
              auto rv = bit->second(args);
              push(std::move(rv));
              break;
            }
            const auto& meta = it->second;

            // Reuse current frame instead of creating new one
            Frame& currentFrame = frames.back();
            currentFrame.ret_ip = currentFrame.ret_ip; // Keep original return address
            currentFrame.isVoid = meta.isVoid;
            currentFrame.typed = meta.typed;
            currentFrame.ret = meta.ret;

            // Create new environment for the tail call
            Env* callerEnv = frames[frames.size()-2].env.get();
            currentFrame.env = std::make_unique<Env>(callerEnv);

            // Bind parameters
            for (size_t i=0; i<meta.params.size() && i<args.size(); ++i) {
              currentFrame.env->Env::Declare(meta.params[i],args[i].t,args[i],0);
            }

            jump(meta.entry);
          } break;

          case SUB: {
            auto b = pop(), a = pop();
            if ((a.t == Type::Int || a.t == Type::Float) && (b.t == Type::Int || b.t == Type::Float)) {
              if (a.t == Type::Float || b.t == Type::Float) {
                push(Value::F(a.AsFloat(p.i) - b.AsFloat(p.i)));
              } else {
                push(Value::I(a.AsInt(p.i) - b.AsInt(p.i)));
              }
            } else {
              Loc L = locate(p.i);
              CString msg = CString("Cannot subtract values of type ") + TypeName(a.t) + " and " + TypeName(b.t);
              ERR(L, msg.c_str());
            }
          } break;

          case MUL: {
            auto b = pop(), a = pop();
            if (a.t==Type::Float || b.t==Type::Float) push(Value::F(a.AsFloat(p.i)*b.AsFloat(p.i)));
            else push(Value::I(a.AsInt(p.i)*b.AsInt(p.i)));
          } break;

          case DIV: {
            auto b = pop(), a = pop();
            push(Value::F(a.AsFloat(p.i)/b.AsFloat(p.i)));
          } break;

          case EQ: {
            auto b = pop(), a = pop();
            bool eq = (a.t==b.t) ? (a==b)
                      : ((a.t!=Type::Str && a.t!=Type::List && b.t!=Type::Str && b.t!=Type::List)
                          ? (a.AsFloat(p.i)==b.AsFloat(p.i)) : false);
            push(Value::B(eq));
          } break;

          case NE: {
            auto b = pop(), a = pop();
            bool ne = (a.t==b.t) ? !(a==b)
                      : ((a.t!=Type::Str && a.t!=Type::List && b.t!=Type::Str && b.t!=Type::List)
                          ? (a.AsFloat(p.i)!=b.AsFloat(p.i)) : true);
            push(Value::B(ne));
          } break;

          case LT: {
            auto b = pop(), a = pop();
            if (a.t==Type::Str && b.t==Type::Str) push(Value::B(std::strcmp(a.AsStr(), b.AsStr()) < 0));
            else push(Value::B(a.AsFloat(p.i)<b.AsFloat(p.i)));
          } break;

          case LE: {
            auto b = pop(), a = pop();
            if (a.t==Type::Str && b.t==Type::Str) push(Value::B(std::strcmp(a.AsStr(), b.AsStr()) <= 0));
            else push(Value::B(a.AsFloat(p.i)<=b.AsFloat(p.i)));
          } break;

          case AND: { auto b = pop(), a = pop(); push(Value::B(a.AsBool(p.i) && b.AsBool(p.i))); } break;
          case OR : { auto b = pop(), a = pop(); push(Value::B(a.AsBool(p.i) || b.AsBool(p.i))); } break;

          case JMP: { auto tgt = fetch64(); jump(tgt); } break;
          case JF : { auto tgt = fetch64(); auto v = pop(); if (!v.AsBool(p.i)) jump(tgt); } break;

          case YIELD: {
            #ifdef _WIN32
              _getch();
            #else
              system("read -n 1");
            #endif
          } break;
          case CALL: {
            auto name = fetchStr();
            uint64_t argc = fetch64();
            std::vector<Value> args(argc);
            for (size_t i=0;i<argc;++i) args[argc-1-i] = pop();

            auto it = fnEntry.find(name);
            if (it == fnEntry.end()) {
              auto bit = builtins.find(name);
              if (bit == builtins.end()) {
                Loc L = locate(p.i);
                ERR(L, "unknown function");
              }
              auto rv = bit->second(args);
              push(std::move(rv));
              break;
            }
            const auto& meta = it->second;

            frames.push_back(Frame{ ip, nullptr, meta.isVoid, meta.typed, meta.ret });
            Env* callerEnv = frames[frames.size()-2].env.get();
            frames.back().env = std::make_unique<Env>(callerEnv);

            // bind parameters
            for (size_t i=0; i<meta.params.size() && i<args.size(); ++i) {
              frames.back().env->Env::Declare(meta.params[i],args[i].t,args[i],0);
            }

            jump(meta.entry);
          } break;

          case RET: {
            Value rv = pop();
            if (frames.size()==1) return;
            if (frames.back().typed) { Coerce(frames.back().ret, rv); }
            uint64_t ret = frames.back().ret_ip;
            frames.pop_back();
            jump(ret);
            push(rv);
          } break;

          case INDEX: {
            auto idxV = pop();
            auto base = pop();
            long long i = idxV.AsInt(p.i);
            if (base.t == Type::List) {
              auto& xs = std::get<std::vector<Value>>(base.v);
              if (i < 0 || (size_t)i >= xs.size()) {
                Loc L = locate(p.i);
                ERR(L, "index out of bounds");
              }
              push(xs[(size_t)i]);
            } else if (base.t == Type::Str) {
              const char* s = base.AsStr();
              size_t len = std::strlen(s);
              if (i < 0 || (size_t)i >= len) {
                Loc L = locate(p.i);
                ERR(L, "index out of bounds");
              }
              char single[2] = {s[i], '\0'};
              push(Value::S(single));
            } else {
              Loc L = locate(p.i);
              CString msg = CString("expected list/string, got ") + TypeName(base.t);
              ERR(L, msg.c_str());
            }
          } break;

          case RET_VOID: {
            if (frames.size()==1) return;
            uint64_t ret = frames.back().ret_ip;
            frames.pop_back();
            jump(ret);
            push(Value::I(0));  // VOID fn: dummy for trailing POP
          } break;

          default:
            {
              Loc L = locate(p.i);
              ERR(L, "bad opcode");
            }
        } // switch
      } // for
    } // run()
  };

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