// This will eventually, all be turned into multiple organized files
// making it easier to develop
// If something is super huge, like importing will be, it will get it's own folder
// Things that go together won't be split up

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <memory>
#include <functional>


#ifdef _WIN32
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
#endif

#include "err.hpp"

namespace minis {

// Global configuration
struct Config {
  struct With {
    bool one_per_while = true;
    bool require_return = true;
    bool forbid_while = true;
  } with;
};

static Config g_cfg;

/*=============================
=           Errors            =
=============================*/
struct Span{ size_t beg=0,end=0; };
struct ScriptError : std::runtime_error{
  Span span; std::vector<std::string> notes;
  explicit ScriptError(std::string m, Span s=Span{}): std::runtime_error(std::move(m)), span(s) {}
};

inline thread_local Span curSpan{};

/*=============================
=       Source / Scanner      =
=============================*/
struct Pos{ size_t i=0; const std::string* src=nullptr; };
inline const ::Source* src = nullptr;

static std::vector<size_t> g_posmap;
static inline size_t map_pos(size_t i) {
  return (i < g_posmap.size()) ? g_posmap[i] : i;
}

inline bool AtEnd(Pos&p){ return p.i>=p.src->size(); }
inline bool IsIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
inline bool IsIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

inline void SkipWS(Pos& p){
  const std::string& s = *p.src;
  for(;;){
    // 1) whitespace
    while (p.i < s.size() && std::isspace((unsigned char)s[p.i])) ++p.i;
    if (p.i >= s.size()) break;

    // 2) // line comment
    if (p.i + 1 < s.size() && s[p.i] == '/' && s[p.i + 1] == '/') {
      p.i += 2;
      while (p.i < s.size() && s[p.i] != '\n') ++p.i;
      continue;
    }

    // 3) /* block comment */
    if (p.i + 1 < s.size() && s[p.i] == '/' && s[p.i + 1] == '*') {
      p.i += 2;
      int depth = 1;
      while (p.i + 1 < s.size() && depth > 0) {
        if (s[p.i] == '/' && s[p.i + 1] == '*') { depth++; p.i += 2; continue; }
        if (s[p.i] == '*' && s[p.i + 1] == '/') { depth--; p.i += 2; continue; }
        ++p.i;
      }
      continue;
    }

    break;
  }
}

inline bool StartsWithKW(Pos&p,const char*kw){
  SkipWS(p); size_t s=p.i, L=strlen(kw); if(s+L>p.src->size()) return false;
  if(p.src->compare(s,L,kw)!=0) return false;
  auto iscont=[](char c){return std::isalnum((unsigned char)c)||c=='_';};
  bool leftOk=(s==0)||!iscont((*p.src)[s-1]);
  bool rightOk=(s+L>=p.src->size())||!iscont((*p.src)[s+L]);
  return leftOk && rightOk;
}

inline long long LineGrab(Pos& P){
  size_t s = P.i, L = 0;
  if (!AtEnd(P) && IsIdCont((*P.src)[s])) {
    while (!AtEnd(P) && IsIdCont((*P.src)[s])) {
      ++L;
      ++s;
    }
  }
  return L;
}

inline bool Match(Pos&p,char c){ SkipWS(p); if(!AtEnd(p)&&(*p.src)[p.i]==c){++p.i; return true;} return false; }
inline bool MatchStr(Pos&p,const char*s){ SkipWS(p); size_t L=strlen(s); if(p.i+L<=p.src->size() && p.src->compare(p.i,L,s)==0){ p.i+=L; return true;} return false; }
inline void Expect(Pos&p,char c){
  SkipWS(p); size_t w=p.i; if(AtEnd(p)||(*p.src)[p.i]!=c) MINIS_ERR("{P2}", *src, p.i, std::string("expected '") + std::string(1, c) + std::string("'")); ++p.i;
}

inline std::string ParseIdent(Pos&p){
  SkipWS(p); size_t s=p.i; if(AtEnd(p)||!IsIdStart((*p.src)[p.i])) MINIS_ERR("{P2}", *src, p.i, "expected identifier");
  ++p.i; while(!AtEnd(p)&&IsIdCont((*p.src)[p.i])) ++p.i; return p.src->substr(s,p.i-s);
}

inline std::string ParseQuoted(Pos&p){
  SkipWS(p); if(AtEnd(p)) MINIS_ERR("{P2}", *src, p.i, "expected string");
  char q=(*p.src)[p.i]; if(q!='"'&&q!='\'') MINIS_ERR("{P2}", *src, p.i, "expeceted string");
  ++p.i; std::string out;
  while(!AtEnd(p)&&(*p.src)[p.i]!=q){
    char c=(*p.src)[p.i++]; if(c=='\\'){ if(AtEnd(p)) MINIS_ERR("{P2}", *src, p.i, "unterminated string; expected '\"'"); char n=(*p.src)[p.i++];
      switch(n){ case 'n':out.push_back('\n');break; case 't':out.push_back('\t');break; case 'r':out.push_back('\r');break;
        case'\\':out.push_back('\\');break; case'"':out.push_back('"');break; case'\'':out.push_back('\'');break;
        default: out.push_back(n); break; }
    } else out.push_back(c);
  }
  if(AtEnd(p)||(*p.src)[p.i]!=q) MINIS_ERR("{P2}", *src, p.i, "unterminated string; expected '\"'"); ++p.i; return out;
}

inline std::string ParseNumberText(Pos&p){
  SkipWS(p); size_t s=p.i; if(!AtEnd(p)&&(((*p.src)[p.i]=='+')||((*p.src)[p.i]=='-'))) ++p.i;
  bool dig=false,dot=false; while(!AtEnd(p)){ char c=(*p.src)[p.i];
    if(std::isdigit((unsigned char)c)){dig=true;++p.i;} else if(c=='.'&&!dot){dot=true;++p.i;} else break;}
  if(!dig) MINIS_ERR("{P2}", *src, p.i, "expected int"); return p.src->substr(s,p.i-s);
}

/*=============================
=          Values/Env         =
=============================*/
enum class Type{ Int=0, Float=1, Bool=2, Str=3, List=4, Null=5 };
inline const char* TypeName(Type t){
  switch(t){
    case Type::Int:  return "int";
    case Type::Float:return "float";
    case Type::Bool: return "bool";
    case Type::Str:  return "str";
    case Type::List: return "list";
    case Type::Null: return "null";
  }
  return "?";
}

struct Value{
  Type t;
  std::variant<long long,double,bool,std::string,std::vector<Value>> v;

  static Value I(long long x){ return {Type::Int,x}; }
  static Value F(double x){ return {Type::Float,x}; }
  static Value B(bool x){ return {Type::Bool,x}; }
  static Value S(std::string s){ return {Type::Str,std::move(s)}; }
  static Value L(std::vector<Value> xs){ return {Type::List,std::move(xs)}; }
  static Value N(){ return {Type::Null, 0LL}; }

   long long AsInt(auto loc) const {
       switch(t){
         case Type::Int:   return std::get<long long>(v);
         case Type::Float: return (long long)std::get<double>(v);
         case Type::Bool:  return std::get<bool>(v) ? 1 : 0;
         case Type::Null:  return 0;
         case Type::Str: {
           const std::string& s = std::get<std::string>(v);
           try {
             return std::stoll(s);
           } catch (...) {
             MINIS_ERR("{S4}", *src, (size_t)loc, "cannot convert string '" + s + "' to int (must be a valid number)");
           }
         }
         case Type::List: MINIS_ERR("{S4}", *src, (size_t)loc, "cannot convert list to int");
         default: MINIS_ERR("{305}", *src, (size_t)loc, "unexpected error");
        }
    }

    double AsFloat(auto loc) const {
        switch(t){
            case Type::Int:   return (double)std::get<long long>(v);
            case Type::Float: return std::get<double>(v);
            case Type::Bool:  return std::get<bool>(v) ? 1.0 : 0.0;
            case Type::Null:  return 0.0;
            case Type::List:  MINIS_ERR("{S4}", *src, (size_t)loc, "cannot convert list to float");
            case Type::Str: {
              const std::string& s = std::get<std::string>(v);
              try {
                return std::stod(s);
              } catch (...) {
                MINIS_ERR("{S4}", *src, (size_t)loc, std::string("cannot convert string '") + s + "' to float");
              }
            }
            default: MINIS_ERR("{305}", *src, (size_t)loc, "unexpected error");
        }
    }

    bool AsBool(auto loc) const {
        switch(t){
            case Type::Bool:  return std::get<bool>(v);
            case Type::Int:   return std::get<long long>(v)!=0;
            case Type::Float: return std::get<double>(v)!=0.0;
            case Type::Str: {
                const std::string& s = std::get<std::string>(v);
                if (s == "true") return true;
                if (s == "false") return false;
                MINIS_ERR("{S4}", *src, (size_t)loc, std::string("cannot convert string '") + s + "' to bool");
            }
            case Type::List:  return !std::get<std::vector<Value>>(v).empty();
            case Type::Null:  return false;
            default: MINIS_ERR("{305}", *src, (size_t)loc, "unexpected error");
        }
    }

    std::string AsStr() const {
        switch(t){
            case Type::Str:   return std::get<std::string>(v);
            case Type::Int:   return std::to_string(std::get<long long>(v));
            case Type::Float: { std::ostringstream os; os<<std::get<double>(v); return os.str(); }
            case Type::Bool:  return std::get<bool>(v) ? "true" : "false";
            case Type::Null:  return "null";
            case Type::List:  {
                const auto& xs = std::get<std::vector<Value>>(v);
                std::ostringstream os; os << "[";
                for (size_t i=0;i<xs.size();++i){ if(i) os<<","; os<<xs[i].AsStr(); }
                os << "]";
                return os.str();
            }
        }
        return {};
    }

    const std::vector<Value>& AsList() const {
        return std::get<std::vector<Value>>(v);
    }
};

// Built-in function handler signature
using BuiltinFn = std::function<Value(std::vector<Value>&)>;

// Registry of built-in functions
inline static Pos p;
inline static std::unordered_map<std::string, BuiltinFn> builtins = {
  {"print", [](std::vector<Value>& args) {
    for (const auto& arg : args) std::cout << arg.AsStr() << " ";
    std::cout << std::endl;
    return Value::N();
  }},
  {"abs", [](std::vector<Value>& args){
    if (args.size() != 1) MINIS_ERR("{BP2}", *src, p.i, "abs requires excatly one argument");
    auto val = args[0];
    if (val.t == Type::Float) return Value::F(std::abs(val.AsFloat(p.i)));
    return Value::I(std::abs(val.AsInt(p.i)));
  }},
  {"neg", [](std::vector<Value>& args) {
    if (args.size() != 1) MINIS_ERR("{BP2}", *src, p.i, "neg requires excatly one argument");
    auto val = args[0];
    if (val.t == Type::Float) return Value::F(-val.AsFloat(p.i));
    return Value::I(-val.AsInt(p.i));
  }},
  {"range", [](std::vector<Value>& args){
    if (args.empty() || args.size() > 2) {
      MINIS_ERR("{BP2}", *src, p.i, "range expects 1-2 arguments");
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
    if (args.empty()) MINIS_ERR("{BP2}", *src, p.i, "max requires at least one argument");
    Value max = args[0];
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i].AsFloat(p.i) > max.AsFloat(p.i)) max = args[i];
    }
    return max;
  }},
  {"min", [](std::vector<Value>& args) {
    if (args.empty()) MINIS_ERR("{BP2}", *src, p.i, "min requires at least one argument");
    Value min = args[0];
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i].AsFloat(p.i) < min.AsFloat(p.i)) min = args[i];
    }
    return min;
  }},
  {"sort", [](std::vector<Value>& args) {
    if (args.size() != 1 || args[0].t != Type::List)
      MINIS_ERR("{BP4}", *src, p.i, "sort requires one list argument");
    std::vector<Value> list = args[0].AsList();
    std::sort(list.begin(), list.end(),
      [](const Value& a, const Value& b) { return a.AsFloat(0) < b.AsFloat(0); });
    return Value::L(list);
  }},
  {"reverse", [](std::vector<Value>& args) {
    if (args.size() != 1) MINIS_ERR("{BP2}", *src, p.i, "reverse requires one argument");
    if (args[0].t == Type::List) {
      std::vector<Value> list = args[0].AsList();
      std::reverse(list.begin(), list.end());
      return Value::L(list);
    } else if (args[0].t == Type::Str) {
      std::string str = args[0].AsStr();
      std::reverse(str.begin(), str.end());
      return Value::S(str);
    }
    MINIS_ERR("{BP4}", *src, p.i, "reverse requires list or string argument");
  }},
  {"sum", [](std::vector<Value>& args) {
    if (args.size() != 1 || args[0].t != Type::List)
      MINIS_ERR("{BP4}", *src, p.i, "sum requires one list argument");
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
    std::string input;
    std::getline(std::cin, input);
    return Value::S(input);
  }},
  {"len", [](std::vector<Value>& args) {
    if (args.size() != 1) MINIS_ERR("{P2}", *src, p.i, "len requires excatly one argument");
    const auto& arg = args[0];
    if (arg.t == Type::List) return Value::I(std::get<std::vector<Value>>(arg.v).size());
    else if (arg.t == Type::Str) return Value::I(std::get<std::string>(arg.v).size());
    MINIS_ERR("{BS4}", *src, p.i, "len requires a list or string");
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

  std::unordered_map<std::string,Var> m; Env* parent=nullptr;

  Value val;

  explicit Env(Env*p=nullptr) : parent(p), val{} {}

  bool ExistsLocal(const std::string&n)const{ return m.find(n)!=m.end();}

  bool Exists(const std::string&n)const{ return ExistsLocal(n)||(parent && parent->Exists(n));}

  const Var& Get(const std::string&n, const int loc)const{
    auto it=m.find(n);
    if(it!=m.end()) return it->second;
    if(parent) return parent->Get(n, loc);
    MINIS_ERR("{P3}", *src, (size_t)loc, std::string("unknown variable '" + n + "'"));
  }

  void Declare(const std::string&n,Type t,Value v){
    if(m.count(n)) MINIS_ERR("{S3}", *src, p.i, "variable already declared");
    Coerce(t,v); m.emplace(n,Var{t,v});
  }

  void Set(const std::string&n,Value v){
    auto it=m.find(n); if(it!=m.end()){ Coerce(it->second.declared,v); it->second.val=v; return; }
    if(parent){ parent->Set(n,v); return; } MINIS_ERR("{BS3}", *src, p.i, "unknown variable");
  }

  void SetOrDeclare(const std::string&n,Value v){ if(ExistsLocal(n)) Set(n,v); else if(parent&&parent->Exists(n)) parent->Set(n,v); else Declare(n,v.t,v); }

  bool Erase(const std::string&n){ return m.erase(n)!=0; }

  bool Unset(const std::string& n){
    if (Erase(n)) return true;
    return parent ? parent->Unset(n) : false;
  }

};

inline bool operator==(const Value&a,const Value&b){
  if(a.t!=b.t) return false;
  switch(a.t){ case Type::Int:return std::get<long long>(a.v)==std::get<long long>(b.v);
    case Type::Float:return std::get<double>(a.v)==std::get<double>(b.v);
    case Type::Bool:return std::get<bool>(a.v)==std::get<bool>(b.v);
    case Type::Str:return std::get<std::string>(a.v)==std::get<std::string>(b.v);
    case Type::List:return std::get<std::vector<Value>>(a.v)==std::get<std::vector<Value>>(b.v);}
  return false;
}

/*=============================
=           Bytecode          =
=============================*/
enum Op : std::uint16_t {
  IMPORTED_FUNC  = 0xE0, // 224
  IMPORTED_LOAD  = 0xE1, // 225
  IMPORTED_STORE = 0xE2, // 226
  NOP            = 0xE3, // 227
  PUSH_I         = 0xE4, // 228
  PUSH_F         = 0xE5, // 229
  PUSH_B         = 0xE6, // 230
  PUSH_S         = 0xE7, // 231
  PUSH_C         = 0xE8, // 232
  MAKE_LIST      = 0xE9, // 233
  GET            = 0xEA, // 234
  SET            = 0xEB, // 235
  DECL           = 0xEC, // 236
  POP            = 0xED, // 237
  ADD            = 0xEE, // 238
  SUB            = 0xEF, // 239
  MUL            = 0xF0, // 240
  DIV            = 0xF1, // 241
  EQ             = 0xF2, // 242
  NE             = 0xF3, // 243
  LT             = 0xF4, // 244
  LE             = 0xF5, // 245
  AND            = 0xF6, // 246
  OR             = 0xF7, // 247
  JMP            = 0xF8, // 248
  JF             = 0xF9, // 249
  CALL           = 0xFA, // 250
  RET            = 0xFB, // 251
  RET_VOID       = 0xFC, // 252
  HALT           = 0xFD, // 253
  UNSET          = 0xFF, // 254
  SLICE          = 0x100,// 255
  INDEX          = 0x101,// 256
  SET_INDEX      = 0x102,// 257
  TAIL           = 0x103,// 258
  YIELD          = 0x104 //259
};

struct FnInfo{
  std::string name;
  uint64_t entry=0;
  std::vector<std::string> params;
  bool isVoid=false;
  bool typed=false; // return type explicitly declared (non-void)
  Type ret=Type::Int;
  bool isInline=false;
  bool tail=false;
  std::vector<std::pair<Type, std::optional<Value>>> param_types;
};

// IO helpers (inline static to avoid ODR)
inline static void write_u8 (FILE*f, uint8_t  v){ fwrite(&v,1,1,f); }
inline static void write_u64(FILE*f, uint64_t v){ fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64_t  v){ fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const std::string&s){ uint64_t n=s.size(); write_u64(f,n); if(n) fwrite(s.data(),1,n,f); }

inline static uint8_t  read_u8 (FILE*f){ uint8_t v;  fread(&v,1,1,f); return v; }
inline static uint64_t read_u64(FILE*f){ uint64_t v; fread(&v,8,1,f); return v; }
inline static std::string read_str(FILE*f){ uint64_t n=read_u64(f); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); return s; }

/*=============================
=           Compiler          =
=============================*/
struct Compiler {
  Pos P;
  FILE* out=nullptr;
  std::vector<FnInfo> fns;
  std::unordered_map<std::string,size_t> fnIndex;
  bool inWith;
  // AST node types
  enum class ASTKind {
    Literal,
    Identifier,
    BinaryExpr,
    UnaryExpr,
    CallExpr,
    FuncDecl,
    VarDecl,
    ReturnStmt,
    IfStmt,
    WhileStmt,
    Block,
  };

  // Base AST node
  struct ASTNode {
    ASTKind kind;
    Type type;          // Static type information
    Span span;          // Source location
    std::vector<std::unique_ptr<ASTNode>> children;
    
    ASTNode(ASTKind k, Type t, Span s) : kind(k), type(t), span(s) {}
    virtual ~ASTNode() = default;
  };

  // Type checking context
  struct TypeContext {
    std::unordered_map<std::string, Type> vars;
    std::unordered_map<std::string, FnInfo> funcs;
    
    bool isCompatible(Type a, Type b) {
      if (a == b) return true;
      if (a == Type::Float && b == Type::Int) return true;
      return false;
    }
    
    void declare(const std::string& name, Type t) {
      if (vars.count(name)) {
        MINIS_ERR("{T1}", *src, p.i, "redefinition of variable");
      }
      vars[name] = t;
    }
    
    Type lookup(const std::string& name) {
      auto it = vars.find(name);
      if (it == vars.end()) {
        MINIS_ERR("{T2}", *src, p.i, "undefined variable"); 
      }
      return it->second;
    }
  };

  // Type checking phase
  struct TypeChecker {
    TypeContext ctx;
    
    void check(ASTNode& node) {
      switch(node.kind) {
        case ASTKind::BinaryExpr: {
          auto& left = *node.children[0];
          auto& right = *node.children[1];
          check(left);
          check(right);
          
          if (!ctx.isCompatible(left.type, right.type)) {
            MINIS_ERR("{T3}", *src, p.i, "type mismatch in binary expression");
          }
          
          // Determine result type based on operation
          if (left.type == Type::Float || right.type == Type::Float) {
            node.type = Type::Float;
          } else {
            node.type = left.type;
          }
          break;
        }
        
        case ASTKind::FuncDecl: {
          auto& retType = node.type;
          if (retType == Type::Null) {
            MINIS_ERR("{T4}", *src, p.i, "function cannot return null");
          }
          
          // Create new scope for function body
          TypeContext outerCtx = std::move(ctx);
          ctx = TypeContext{};
          
          // Check body with new scope
          for (auto& child : node.children) {
            check(*child);
          }
          
          ctx = std::move(outerCtx);
          break;
        }
        
        // Add more cases as needed
      }
    }
  };

  // minis header fields
  uint64_t table_offset_pos=0, fn_count_pos=0, entry_main_pos=0;

  explicit Compiler(const ::Source& s) { minis::src = &s; P.src = &s.text; }

  inline Type parseType(){
    if(StartsWithKW(P,"int")){ P.i+=3; return Type::Int;}
    if(StartsWithKW(P,"float")){ P.i+=5; return Type::Float;}
    if(StartsWithKW(P,"bool")){ P.i+=4; return Type::Bool;}
    if(StartsWithKW(P,"str")){ P.i+=3; return Type::Str;}
    if(StartsWithKW(P,"list")){ P.i+=4; return Type::List;}
    if(StartsWithKW(P, "null")){ P.i+=4; return Type::Null;}
    MINIS_ERR("{S5}", *src, P.i, "unknown type (use int|float|bool|str|list|null)");
  }

  inline void emit_u8 (uint8_t v){ write_u8(out,v); }
  inline void emit_u64(uint64_t v){ write_u64(out,v); }
  inline void emit_s64(int64_t v){ write_s64(out,v); }
  inline void emit_f64(double v){ write_f64(out,v); }
  inline void emit_str(const std::string&s){ write_str(out,s); }
  inline uint64_t tell() const { return (uint64_t)ftell(out); }
  inline void seek(uint64_t pos){ fseek(out,(long)pos,SEEK_SET); }

  // --- Expressions -> bytecode ---
  inline void Expr(){ LogicOr(); }
  inline void LogicOr(){ LogicAnd(); while(MatchStr(P,"||")){ LogicAnd(); emit_u64(OR); } }
  inline void LogicAnd(){ Equality(); while(MatchStr(P,"&&")){ Equality(); emit_u64(AND); } }
  inline void Equality(){
    AddSub();
    while(true){
      if(MatchStr(P,"==")){ AddSub(); emit_u64(EQ); }
      else if(MatchStr(P,"!=")){ AddSub(); emit_u64(NE); }
      else if(MatchStr(P,">=")){ AddSub(); emit_u64(LE); emit_u64(MUL); emit_u64(PUSH_I); emit_s64(-1); emit_u64(MUL); }
      else if(MatchStr(P,">")) { AddSub(); emit_u64(LT); emit_u64(MUL); emit_u64(PUSH_I); emit_s64(-1); emit_u64(MUL); }
      else if(MatchStr(P,"<=")){ AddSub(); emit_u64(LE); }
      else if(MatchStr(P,"<")) { AddSub(); emit_u64(LT); }
      else break;
    }
  }
  inline void AddSub(){
    MulDiv();
    while(true){
      if(Match(P,'+')){ MulDiv(); emit_u64(ADD); }
      else if(Match(P,'-')){ MulDiv(); emit_u64(SUB); }
      else break;
    }
  }
  inline void MulDiv(){
    Factor();
    while(true){
      if(Match(P,'*')){ Factor(); emit_u64(MUL); }
      else if(Match(P,'/')){ Factor(); emit_u64(DIV); }
      else break;
    }
  }
  inline void ListLit(){
    size_t count=0;
    if(Match(P,']')){ emit_u64(MAKE_LIST); emit_u64(0); return; }
    while(true){ Expr(); ++count; SkipWS(P); if(Match(P,']')) break; Expect(P,','); }
    emit_u64(MAKE_LIST); emit_u64(count);
  }
  inline void Factor(){
    SkipWS(P);
    if(!AtEnd(P)&&(*P.src)[P.i]=='('){ ++P.i; Expr(); Expect(P,')'); return; }
    if(!AtEnd(P) && ((*P.src)[P.i]=='"'||(*P.src)[P.i]=='\'')){ auto s=ParseQuoted(P); emit_u64(PUSH_S); emit_str(s); return; }
    if(P.i+4<=P.src->size() && P.src->compare(P.i,4,"true")==0 && (P.i+4==P.src->size()||!IsIdCont((*P.src)[P.i+4]))){ P.i+=4; emit_u64(PUSH_B); emit_u8(1); return; }
    if(P.i+5<=P.src->size() && P.src->compare(P.i,5,"false")==0&& (P.i+5==P.src->size()||!IsIdCont((*P.src)[P.i+5]))){ P.i+=5; emit_u64(PUSH_B); emit_u8(0); return; }
    if(!AtEnd(P)&&(*P.src)[P.i]=='['){ ++P.i; ListLit(); return; }
    if(!AtEnd(P)&&(std::isdigit((unsigned char)(*P.src)[P.i])||(*P.src)[P.i]=='+'||(*P.src)[P.i]=='-')){
      auto s=ParseNumberText(P); if(s.find('.')!=std::string::npos){ emit_u64(PUSH_F); emit_f64(std::stod(s)); }
      else{ emit_u64(PUSH_I); emit_s64(std::stoll(s)); } return;
    }
    if(!AtEnd(P)&&IsIdStart((*P.src)[P.i])){
      auto id=ParseIdent(P); SkipWS(P);
      if(!AtEnd(P)&&(*P.src)[P.i]=='('){ // call
        ++P.i; size_t argc=0;
        if(!Match(P,')')){
          while(true){
            Expr(); ++argc; SkipWS(P);
            if(Match(P,')')) break;
            Expect(P,',');
          }
        }
        emit_u64(CALL);
        emit_str(id);
        emit_u64(argc);
        return;
      } else {
        emit_u64(GET);
        emit_str(id);
        return;
      }
    for(;;) {
      SkipWS(P);
      if (!Match(P, '[')) break;
      Expr();
      Expect(P, ']');
      emit_u64(INDEX);
    }
    MINIS_ERR("{P?}", *src, P.i, "unexpected token in expression");
    }
  }

  // ---- Statements -> bytecode ----
  struct LoopLbl {
    uint64_t condOff=0, contTarget=0;
    std::vector<uint64_t> breakPatchSites;
  };
  std::vector<LoopLbl> loopStack;

  inline void patchJump(uint64_t at, uint64_t target){ auto cur=tell(); seek(at); write_u64(out,target); seek(cur); }

  inline void StmtSeq(){
    while(true){
      SkipWS(P); if(AtEnd(P)) break;
      if (!AtEnd(P) && (*P.src)[P.i] == '}') break;

      if (!AtEnd(P) && (*P.src)[P.i] == '{') {
        ++P.i;
        StmtSeqUntilBrace();
        continue;
      }
      else if(StartsWithKW(P,"exit")){ P.i+=4; Expect(P,';'); emit_u64(HALT); continue; }

      else if(StartsWithKW(P,"import")){ P.i+=6; SkipWS(P);
        if(!AtEnd(P)&&((*P.src)[P.i]=='"'||(*P.src)[P.i]=='\'')) (void)ParseQuoted(P); else (void)ParseIdent(P);
        Expect(P,';'); continue; }

      else if(StartsWithKW(P,"del")){ P.i+=3; SkipWS(P); auto n=ParseIdent(P); Expect(P,';');
        emit_u64(UNSET); emit_str(n); continue; }

      else if(StartsWithKW(P,"return")){
        P.i+=6; SkipWS(P);
        if(Match(P,';')){ emit_u64(RET_VOID); continue; }
        Expr(); Expect(P,';'); emit_u64(RET); continue;
      }

      else if(MatchStr(P,"++")) {
        SkipWS(P);
        auto name = ParseIdent(P);
        Expect(P,';');
        emit_u64(GET);  emit_str(name);
        emit_u64(PUSH_I); emit_s64(1);
        emit_u64(ADD);
        emit_u64(SET);  emit_str(name);
        continue;
      }

      else if(StartsWithKW(P,"continue")){
        P.i+=8; SkipWS(P); Expect(P,';');
        if(loopStack.empty()) MINIS_ERR("{V5}", *src, P.i, "'continue' outside of loop");
        emit_u64(JMP); emit_u64(loopStack.back().contTarget);
        continue;
      }

      else if(StartsWithKW(P,(const char*)"break")){
        P.i+=5; size_t levels=1; SkipWS(P);
        if(!AtEnd(P) && std::isdigit((unsigned char)(*P.src)[P.i])){ auto num=ParseNumberText(P); levels=(size_t)std::stoll(num); }
        Expect(P,';');
        if(loopStack.size()<levels) MINIS_ERR("{V5}", *src, P.i, "'break' outside of loop");
        size_t idx=loopStack.size()-levels;
        emit_u64(JMP); auto at=tell(); emit_u64(0);
        loopStack[idx].breakPatchSites.push_back(at);
        continue;
      }

      else if(StartsWithKW(P, "func")) {
        P.i += 4; SkipWS(P);

        // Parse function attributes now as keywords
        bool isInline = false;
        bool tailCallOpt = false;

        if (StartsWithKW(P, "inline")) {
          P.i += 6;
          isInline = true;
          SkipWS(P);
        }
        if (StartsWithKW(P, "tailcall")) {
          P.i += 8;
          tailCallOpt = true;
          SkipWS(P);
        }

        // Track if types were specified for warning
        bool hasExplicitTypes = false;
        bool isVoid = false, typed = false;
        Type rt = Type::Int;
        std::string fname;

        // Return type parsing with better error messages
        Pos look = P;
        if (StartsWithKW(look,"void") || StartsWithKW(look,"int") || StartsWithKW(look,"float") ||
        StartsWithKW(look,"bool") || StartsWithKW(look,"str") || StartsWithKW(look,"list")) {
          hasExplicitTypes = true;
          if (StartsWithKW(P,"void")) { P.i += 4; isVoid = true; }
          else { rt = parseType(); typed = true; }
          SkipWS(P);
        }

        fname = ParseIdent(P);

        // Parameter parsing with Python-style defaults
        SkipWS(P); Expect(P,'(');
        std::vector<std::string> params;
        std::vector<std::pair<Type, std::optional<Value>>> paramTypes; // Track param types & defaults

        SkipWS(P);
        if(!Match(P,')')) {
          while (true) {
        // Check for type annotation
        Pos typeCheck = P;
        Type paramType = Type::Int; // Default type if none specified
        if (StartsWithKW(typeCheck,"int") || StartsWithKW(typeCheck,"float") ||
            StartsWithKW(typeCheck,"bool") || StartsWithKW(typeCheck,"str") ||
            StartsWithKW(typeCheck,"list")) {
          paramType = parseType();
          hasExplicitTypes = true;
          SkipWS(P);
        }

        params.push_back(ParseIdent(P));
        SkipWS(P);

        // Handle default value
        std::optional<Value> defaultVal;
        if (Match(P, '=')) {
          Pos saveP = P;
          try {
            // Parse literal value for default
            if (Match(P, '"') || Match(P, '\'')) {
          defaultVal = Value::S(ParseQuoted(P));
            }
            else if (std::isdigit((unsigned char)(*P.src)[P.i]) ||
             (*P.src)[P.i] == '-' || (*P.src)[P.i] == '+') {
          std::string num = ParseNumberText(P);
          if (num.find('.') != std::string::npos) {
            defaultVal = Value::F(std::stod(num));
          } else {
            defaultVal = Value::I(std::stoll(num));
          }
            }
            else if (StartsWithKW(P, "true")) {
          P.i += 4;
          defaultVal = Value::B(true);
            }
            else if (StartsWithKW(P, "false")) {
          P.i += 5;
          defaultVal = Value::B(false);
            }
          } catch (...) {
            P = saveP;
          }
        }

        paramTypes.push_back({paramType, defaultVal});

        SkipWS(P);
        if (Match(P,')')) break;
        Expect(P,',');
          }
        }

        // Emit type usage warning if needed
        if (!hasExplicitTypes) {
          std::cerr << "Warning: Function '" << fname << "' uses implicit types. "
           << "Consider adding explicit type annotations for better safety and clarity.\n";
        }

        SkipWS(P); Expect(P,'{');

        // Register function
        FnInfo fni{fname, 0, params, isVoid, typed, rt};
        fni.isInline = isInline;
        fni.tail = tailCallOpt;
        fni.param_types = paramTypes;
        size_t idx = fns.size();
        fns.push_back(fni);
        fnIndex[fname] = idx;

        // Function body parsing
        emit_u64(JMP);
        auto skipAt = tell();
        emit_u64(0);

        auto entry = tell();
        fns[idx].entry = entry;

        StmtSeqUntilBrace();

        if (isVoid) emit_u64(RET_VOID);
        else emit_u64(RET);

        auto after = tell();
        patchJump(skipAt, after);
        continue;
      }

      // Conversions
      else if(StartsWithKW(P, "conv")){
        P.i+=4;
        SkipWS(P);
        auto name = ParseIdent(P);
        SkipWS(P); Expect(P,':'); SkipWS(P);
        Type newType = parseType();
        Expect(P,';');

        // redeclare the variable with new type
        emit_u64(DECL);
        emit_str(name);
        emit_u64((uint64_t)newType);
      }

      else if(StartsWithKW(P,"yield")){
        P.i+=5;SkipWS(P);Expect(P,';');SkipWS(P);
        emit_u64(YIELD);
        continue;
      }

      // while (cond) { ... }
      if (StartsWithKW(P,"while")) {
        P.i += 5; SkipWS(P); Expect(P,'('); SkipWS(P);
        auto condOff = tell();
        Expr(); Expect(P,')');
        emit_u64(JF); auto jfAt = tell(); emit_u64(0);

        SkipWS(P); Expect(P,'{');

        // Track loop labels for break/continue
        LoopLbl L{}; L.condOff = condOff; L.contTarget = condOff;
        loopStack.push_back(L);

        // Enforce: at most one 'with' group inside this while
        bool thisWhileHasWith = false;

        // Parse the loop body (balanced braces)
        std::size_t depth = 1;
        while (!AtEnd(P)) {
          if ((*P.src)[P.i] == '{') { ++depth; ++P.i; continue; }
          if ((*P.src)[P.i] == '}') { if (--depth == 0) { ++P.i; break; } ++P.i; continue; }

          // See if next token is 'with'
          Pos peek = P; SkipWS(peek);
          if (StartsWithKW(peek, "with")) {
            if (thisWhileHasWith) {
              MINIS_ERR("{S01}", *src, P.i, "only one 'with' group allowed per 'while'");
            }
            thisWhileHasWith = true;

            // consume 'with'
            P = peek; P.i += 4; SkipWS(P);

            const std::size_t MAX_THREADS = 10;
            std::vector<std::string> bodies; bodies.reserve(4);

            auto parse_one_block = [&](const char* ctx){
              Expect(P, '{');
              std::size_t bdepth = 1, start = P.i;
              while (!AtEnd(P)) {
                char c = (*P.src)[P.i++];
                if (c == '{') { ++bdepth; continue; }
                if (c == '}') {
                  if (--bdepth == 0) {
                    std::size_t end = P.i - 1;
                    std::string body = P.src->substr(start, end - start);

                    auto isId = [](char ch){ return std::isalnum((unsigned char)ch) || ch=='_'; };
                    std::size_t pos = 0;
                    while ((pos = body.find("while", pos)) != std::string::npos) {
                      bool leftOk  = (pos==0) || !isId(body[pos-1]);
                      bool rightOk = (pos+5>=body.size()) || !isId(body[pos+5]);
                      if (leftOk && rightOk) {
                        MINIS_ERR("{S01}", *src, P.i, "no 'while' allowed inside 'with'/'and' block");
                      }
                      ++pos;
                    }

                    bodies.push_back(std::move(body));
                    return;
                  }
                }
              }
              MINIS_ERR("{S02}", *src, P.i, std::string("unterminated '{' in '") + ctx + "' block");
            };

            // First block after 'with'
            parse_one_block("with");

            // Zero or more:  and { ... }
            while (true) {
              Pos pk = P; SkipWS(pk);
              if (!StartsWithKW(pk, "and")) break;
              P = pk; P.i += 3; SkipWS(P);
              parse_one_block("and");
            }

            if (bodies.empty()) {
              MINIS_ERR("{S02}", *src, P.i, "'with' expects at least one block");
            }
            if (bodies.size() > MAX_THREADS) {
              MINIS_ERR("{S01}", *src, P.i,
                "too many 'and' blocks (max " + std::to_string(MAX_THREADS) + ")");
            }

            // For each body: register as function, compile body, then TAIL
            std::vector<std::string> fnNames; fnNames.reserve(bodies.size());
            for (std::size_t i = 0; i < bodies.size(); ++i) {
              std::string fnName = "__with_fn_" + std::to_string(i);
              fnNames.push_back(fnName);

              // register
              FnInfo fni{fnName, 0, /*params*/{}, /*isVoid*/true, /*typed*/false, Type::Null};
              fni.tail = true;
              std::size_t idx = fns.size();
              fns.push_back(fni);
              fnIndex[fnName] = idx;

              // skip body from main
              emit_u64(JMP); auto skipAt = tell(); emit_u64(0);

              // entry
              auto entry = tell();
              fns[idx].entry = entry;

              // compile body by swapping P.src/P.i temporarily
              Pos saveP = P; const std::string* saveSrc = P.src;
              P.src = &bodies[i];
              P.i   = 0;
              StmtSeq();           // reuse your existing statement sequencer
              P.src = saveSrc; P = saveP;

              // force a tail exit to itself (as per your design)
              emit_u64(TAIL);
              emit_str(fnName);
              emit_u64(0);

              // patch skip
              auto afterFn = tell();
              patchJump(skipAt, afterFn);
            }

            // Launch: tailcall each generated function (argc = 0)
            for (auto& fn : fnNames) {
              emit_u64(TAIL);
              emit_str(fn);
              emit_u64(0);
            }

            // cleanup
            for (auto& s : bodies) { s.clear(); s.shrink_to_fit(); }
            bodies.clear(); bodies.shrink_to_fit();
            fnNames.clear(); fnNames.shrink_to_fit();

            continue; // handled 'with' inside while-body
          }

          // Not a 'with' statement → regular statement in while-body
          StmtSeqOne();
        }

        // loop back to condition
        emit_u64(JMP); emit_u64(condOff);

        // patch exits/breaks
        auto after = tell();
        patchJump(jfAt, after);
        for (auto site : loopStack.back().breakPatchSites) patchJump(site, after);
        loopStack.pop_back();

        continue;
      }

      // if (...) { ... } elif (...) { ... } else { ... }
      else if(StartsWithKW(P,"if")){
        P.i+=2; SkipWS(P); Expect(P,'('); Expr(); Expect(P,')');
        emit_u64(JF); auto jfAt = tell(); emit_u64(0);
        SkipWS(P); Expect(P,'{'); StmtSeqUntilBrace();

        emit_u64(JMP); auto jendAt=tell(); emit_u64(0);
        auto afterThen = tell(); patchJump(jfAt, afterThen);

        std::vector<uint64_t> ends; ends.push_back(jendAt);
        while(true){
          Pos peek=P; SkipWS(peek); if(!StartsWithKW(peek,"elif")) break;
          P.i=peek.i+4; SkipWS(P); Expect(P,'('); Expr(); Expect(P,')');
          emit_u64(JF); auto ejf=tell(); emit_u64(0);
          SkipWS(P); Expect(P,'{'); StmtSeqUntilBrace();
          emit_u64(JMP); auto ejend=tell(); emit_u64(0);
          ends.push_back(ejend);
          auto afterElif=tell(); patchJump(ejf, afterElif);
        }
        Pos peek=P; SkipWS(peek);
        if(StartsWithKW(peek,"else")){
          P.i=peek.i+4; SkipWS(P); Expect(P,'{'); StmtSeqUntilBrace();
        }
        auto afterAll=tell();
        for(auto at: ends) patchJump(at, afterAll);
        continue;
      }

      // try { ... } except { ... } finally { ... }
      else if(StartsWithKW(P,"try")){
        P.i+=3;
        SkipWS(P); Expect(P,'{');

        // Store jump points for exception handling
        emit_u64(JMP);
        auto tryStart = tell();
        emit_u64(0);

        // Try block
        StmtSeqUntilBrace();

        emit_u64(JMP);
        auto afterTry = tell();
        emit_u64(0);

        // Exception handler
        auto exceptStart = tell();
        patchJump(tryStart, exceptStart);

        SkipWS(P);
        if(!StartsWithKW(P,"except")) MINIS_ERR("{P2}", *src, p.i, "expected 'except' after try block");
        P.i+=6;
        SkipWS(P); Expect(P,'{');
        StmtSeqUntilBrace();

        auto afterExcept = tell();
        patchJump(afterTry, afterExcept);

        // Optional finally block
        SkipWS(P);
        if(StartsWithKW(P,"finally")){
          P.i+=7;
          SkipWS(P); Expect(P,'{');
          StmtSeqUntilBrace();
        }
        continue;
      }

      // lambda
      else if(StartsWithKW(P,"lambda")){
        P.i+=6; SkipWS(P);

        // Parse parameters
        std::vector<std::string> params;
        if(Match(P,'(')){
          if(!Match(P,')')){
            while(true){
              params.push_back(ParseIdent(P));
              SkipWS(P);
              if(Match(P,')')) break;
              Expect(P,',');
            }
          }
        }

        SkipWS(P); Expect(P,':');

        // Generate unique lambda name
        static int lambdaCount = 0;
        std::string lambdaName = "__lambda_" + std::to_string(lambdaCount++);

        // Register lambda as a function
        FnInfo fni{lambdaName, 0, params, false, false, Type::Int};
        size_t idx = fns.size();
        fns.push_back(fni);
        fnIndex[lambdaName] = idx;

        // Skip lambda body in main code
        emit_u64(JMP);
        auto skipAt = tell();
        emit_u64(0);

        // Lambda function entry point
        auto entry = tell();
        fns[idx].entry = entry;

        // Compile expression body
        Expr();
        emit_u64(RET);

        auto after = tell();
        patchJump(skipAt, after);

        // Push lambda name as a value
        emit_u64(PUSH_S);
        emit_str(lambdaName);

        Expect(P,';');
        continue;
      }

      // error
      else if(StartsWithKW(P, "throw")){
        P.i+=5;SkipWS(P);
        if (Match(P, '"') || Match(P, '\'')) {
          std::string msg = ParseQuoted(P);
          Expect(P, ';');
          throw ScriptError(msg);
        } else {
          std::string errorType = ParseIdent(P);
          if (errorType == "ValueError") {
            std::string msg = "ValueError: Invalid value or type";
            if (Match(P, '(')) {
              msg = ParseQuoted(P);
              Expect(P, ')');
            }
            Expect(P, ';');
            throw ScriptError(msg);
          } else if (errorType == "TypeError") {
            std::string msg = "TypeError: Type mismatch";
            if (Match(P, '(')) {
              msg = ParseQuoted(P);
              Expect(P, ')');
            }
            Expect(P, ';');
            throw ScriptError(msg);
          } else if (errorType == "IndexError") {
            std::string msg = "IndexError: Index out of range";
            if (Match(P, '(')) {
              msg = ParseQuoted(P);
              Expect(P, ')');
            }
            Expect(P, ';');
            throw ScriptError(msg);
          } else if (errorType == "NameError") {
            std::string msg = "NameError: Name not found";
            if (Match(P, '(')) {
              msg = ParseQuoted(P);
              Expect(P, ')');
            }
            Expect(P, ';');
            throw ScriptError(msg);
          } else {
            MINIS_ERR("{P4}", *src, p.i, "error type unknown");
          }
        }
      }

      // let [type|auto] name = expr ;
      else if(StartsWithKW(P,"let")){
        P.i+=3; SkipWS(P);

        // Parse modifiers
        SkipWS(P);
        bool isConst = StartsWithKW(P,"const") ? (P.i+=5, true) : false;
        SkipWS(P);

        bool isStatic = StartsWithKW(P,"static") ? (P.i+=6, true) : false;
        SkipWS(P);

        // Owned is mutually exclusive with const since const implies immutable
        bool isOwned = !isConst && StartsWithKW(P,"owned") ? (P.i+=5, true) : false;
        SkipWS(P);


        bool isShared = StartsWithKW(P,"shared") ? (P.i+=6, true) : false;

        // Parse type

        bool isAuto=false;
        bool isNull=false;
        Type T=Type::Int;

        if(StartsWithKW(P,"auto")) {
          isAuto=true;
          P.i+=4;
        }
        else if(StartsWithKW(P,"null")) {
          isNull=true;
          P.i+=4;
        }
        else {
          T=parseType();
        }

        SkipWS(P);

        if(isOwned && isShared) {
          MINIS_ERR("{S3}", *src, p.i, "variable cannot be both owned and shared");
        }

        SkipWS(P);
        auto name = ParseIdent(P);
        SkipWS(P);

        // Handle initialization
        if(!isNull) {
          Expect(P,'=');
          Expr();
          Expect(P,';');
        } else {
          Expect(P,';');
        }

        // Encode modifiers in high bits of type byte
        uint64_t type_byte = isAuto ? 0xEC : (uint8_t)T;
        if(isConst) type_byte |= 0x100;
        if(isStatic) type_byte |= 0x200;
        if(isOwned) type_byte |= 0x400;
        if(isShared) type_byte |= 0x800;

        emit_u64(DECL);
        emit_str(name);
        emit_u64(type_byte);
        continue;
      }

      // assignment or call;
      else if(!AtEnd(P)&&IsIdStart((*P.src)[P.i])){
        size_t save=P.i; auto name=ParseIdent(P); SkipWS(P);
        if(!AtEnd(P) && (*P.src)[P.i]=='='){
          ++P.i; Expr(); Expect(P,';'); emit_u64(SET); emit_str(name); continue;
        } else {
          P.i=save; Expr(); Expect(P,';'); emit_u64(POP); continue;
        }
      }

      MINIS_ERR("{P1}", *src, P.i, "unexpected token");
      SkipWS(P);
    }
  }

  inline void StmtSeqOne(){ SkipWS(P); if(AtEnd(P)||(*P.src)[P.i]=='}') return; StmtSeq(); }
  inline void StmtSeqUntilBrace(){
    size_t depth=1;
    while(!AtEnd(P)){
      if((*P.src)[P.i]=='}'){ --depth; ++P.i; if(depth==0) break; continue; }
      if((*P.src)[P.i]=='{'){ ++depth; ++P.i; continue; }
      StmtSeqOne();
    }
  }

  inline void writeHeaderPlaceholders(){
    fwrite("AVOCADO1",1,8,out);
    /*write_str(out, "0");     // compiler version
    write_str(out, "0");     // VM version*/
    table_offset_pos = (uint64_t)ftell(out); write_u64(out, 0); // table_offset
    fn_count_pos     = (uint64_t)ftell(out); write_u64(out, 0); // count
    entry_main_pos   = (uint64_t)ftell(out); write_u64(out, 0); // entry_main
  }

  inline void compileToFile(const std::string& outPath) {
    struct VarInfo {
      bool isOwned = true;
      bool isMutable = true;
      bool isUsed = false;
      bool isRef = false;
      std::string owner;
      size_t lastUsed = 0;
      Type type;
      Value initialValue;
    };

    struct Scope {
      std::unordered_map<std::string, VarInfo> vars;
      std::vector<std::string> deadVars;
    };

    std::vector<Scope> scopeStack;
    std::unordered_map<std::string, std::string> varRenames;
    std::vector<std::string> warnings;

    auto generateName = [](size_t n) -> std::string {
      std::string result;
      while (n >= 26) {
        result = (char)('a' + (n % 26)) + result;
        n = n/26 - 1;
      }
      result = (char)('a' + n) + result;
      return result;
    };

    auto trackVarUse = [&](const std::string& name, size_t pos) {
      for (auto& scope : scopeStack) {
        if (auto it = scope.vars.find(name); it != scope.vars.end()) {
          it->second.isUsed = true;
          it->second.lastUsed = pos;
          if (it->second.isRef && !it->second.isMutable) {
            warnings.push_back("Warning: Immutable reference '" + name + "' used");
          }
          break;
        }
      }
    };

    out = fopen(outPath.c_str(), "wb+");
    if (!out) throw std::runtime_error("cannot open bytecode file for write");

    writeHeaderPlaceholders();

    // Initialize main scope
    scopeStack.push_back(Scope{});

    // Top-level as __main__
    FnInfo mainFn{"__main__", 0, {}, true, false, Type::Int};
    fns.push_back(mainFn);
    fnIndex["__main__"] = 0;
    fns[0].entry = tell();

    P.i = 0;
    SkipWS(P);

    // First pass: Collect variable usage info
    auto startPos = P.i;

    // Remove dead code and apply optimizations
    for (const auto& scope : scopeStack) {
      for (const auto& [name, info] : scope.vars) {
        if (!info.isUsed) {
          warnings.push_back("Warning: Unused variable '" + name + "'");
          // Remove related instructions
        }
        if (info.isOwned && !info.owner.empty()) {
          warnings.push_back("Warning: Variable '" + name + "' moved but never used after move");
        }
      }
    }

    // Generate minimal variable names
    size_t nameCounter = 0;
    for (const auto& scope : scopeStack) {
      for (const auto& [name, info] : scope.vars) {
        if (info.isUsed) {
          varRenames[name] = generateName(nameCounter++);
        }
      }
    }

    // Second pass: Generate optimized bytecode
    P.i = startPos;
    StmtSeq();
    emit_u64(HALT);

    // Write function table with optimized names
    uint64_t tbl_off = tell();
    uint64_t count = (uint64_t)fns.size();

    for (auto& fn : fns) {
      write_str(out, varRenames[fn.name]);
      write_u64(out, fn.entry);
      write_u8(out, fn.isVoid ? 1 : 0);
      write_u8(out, fn.typed ? 1 : 0);
      write_u8(out, (uint8_t)fn.ret);
      write_u64(out, (uint64_t)fn.params.size());
      for (const auto& s : fn.params) {
        write_str(out, varRenames[s]);
      }
    }

    // Patch header
    fflush(out);
    seek(table_offset_pos);
    write_u64(out, tbl_off);
    seek(fn_count_pos);
    write_u64(out, count);
    seek(entry_main_pos);
    write_u64(out, fns[0].entry);

    // Print warnings
    for (const auto& warning : warnings) {
      std::cerr << warning << std::endl;
    }

    fclose(out);
    out = nullptr;
  }
};

/*=============================
=              VM            =
=============================*/
struct VM {
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

  struct FnMeta{ uint64_t entry; bool isVoid; bool typed; Type ret; std::vector<std::string> params; };
  std::unordered_map<std::string, FnMeta> fnEntry;

  explicit VM() {}

    inline void jump(uint64_t target){ ip = target; fseek(f,(long)ip,SEEK_SET); }
    inline uint8_t  fetch8(){ uint8_t v; fread(&v,1,1,f); ++ip; return v; }
    inline uint64_t fetch64(){ uint64_t v; fread(&v,8,1,f); ip+=8; return v; }
    inline int64_t  fetchs64(){ int64_t v; fread(&v,8,1,f); ip+=8; return v; }
    inline double   fetchf64(){ double v; fread(&v,8,1,f); ip+=8; return v; }
    inline std::string fetchStr(){ uint64_t n=fetch64(); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); ip+=n; return s; }

    inline Value pop(){
      try {
        if(stack.empty()) MINIS_ERR("{V5}", *src, p.i, "stack underflow");
        if(stack.back().t == Type::Null) MINIS_ERR("{V4}", *src, p.i, "attempt to use null value");
        Value v=std::move(stack.back());
        stack.pop_back();
        return v;
      } catch(const std::exception& e) {
        MINIS_ERR("{V5}", *src, p.i, "stack operation failed: " + std::string(e.what()));
      }
    }
    inline void push(Value v){ stack.push_back(std::move(v)); }
    inline void discard() {
      if (stack.empty()) MINIS_ERR("{S1}", *src, (size_t)ip, "stack underflow");
      stack.pop_back();
    }

    inline void load(const std::string& path){
      f=fopen(path.c_str(),"rb"); if(!f) throw std::runtime_error("cannot open bytecode");
      char magic[9]={0}; fread(magic,1,8,f); if(std::string(magic)!="AVOCADO1") throw std::runtime_error("bad bytecode verification");
      table_off = read_u64(f); uint64_t fnCount = read_u64(f); uint64_t entry_main = read_u64(f);
      ip = entry_main; code_end = table_off;

      fseek(f, (long)table_off, SEEK_SET);
      for(uint64_t i=0;i<fnCount;++i){
        std::string name = read_str(f);
        uint64_t entry   = read_u64(f);
        bool isVoid      = read_u8(f)!=0;
        bool typed       = read_u8(f)!=0;
        Type ret         = (Type)read_u8(f);
        uint64_t pcnt    = read_u64(f);
        std::vector<std::string> params; params.reserve((size_t)pcnt);
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
          frames.back().env->SetOrDeclare(id, v);
        } break;

        case DECL: {
          auto id = fetchStr();
          uint64_t tt = fetch64();
          auto v  = pop();
          if (tt == 0xECull) frames.back().env->Declare(id, v.t, v);
          else            frames.back().env->Declare(id, (Type)tt, v);
        } break;

        case POP: { discard(); } break;

        case ADD: {
          auto b = pop();
          auto a = pop();

          // Handle addition based on type combinations
          if (a.t == Type::Null || b.t == Type::Null) {
            MINIS_ERR("{V04}", *src, p.i, "Cannot perform addition with null values");
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
            push(Value::S(a.AsStr() + b.AsStr())); // String concatenation
          } else if (a.t == Type::Float || b.t == Type::Float) {
            push(Value::F(a.AsFloat(p.i) + b.AsFloat(p.i))); // Any numeric with Float = Float
          } else if (a.t == Type::Int || b.t == Type::Int) {
            push(Value::I(a.AsInt(p.i) + b.AsInt(p.i))); // Int or Bool = Int
          }else if (a.t == Type::Float && b.t == Type::Int || a.t == Type::Int && b.t == Type::Float){
            push(Value::F(a.AsFloat(p.i) + b.AsFloat(p.i)));
          } else {
            MINIS_ERR("{V04}", *src, p.i, std::string("Cannot add values of type ") + TypeName(a.t) + " and " + TypeName(b.t));
          }
        } break;

        case UNSET: {
        auto id = fetchStr();
          if (!frames.back().env->Unset(id))
            MINIS_ERR("{S3}", *src, p.i, "unknown variable");
        } break;

        case TAIL: {
          auto name = fetchStr();
          uint64_t argc = fetch64();
          std::vector<Value> args(argc);
          for (size_t i=0; i<argc; ++i) args[argc-1-i] = pop();

          auto it = fnEntry.find(name);
          if (it == fnEntry.end()) {
            auto bit = minis::builtins.find(name);
            if (bit == builtins.end()) {
          MINIS_ERR("{S3}", *src, p.i, "unknown function");
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
            currentFrame.env->Declare(meta.params[i], args[i].t, args[i]);
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
            MINIS_ERR("{V04}", *src, p.i, std::string("Cannot subtract values of type ") + TypeName(a.t) + " and " + TypeName(b.t));
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
          if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()<b.AsStr()));
          else push(Value::B(a.AsFloat(p.i)<b.AsFloat(p.i)));
        } break;

        case LE: {
          auto b = pop(), a = pop();
          if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()<=b.AsStr()));
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
            auto bit = minis::builtins.find(name);
            if (bit == builtins.end()) {
              MINIS_ERR("{S3}", *src, p.i, "unknown function");
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
            frames.back().env->Declare(meta.params[i], args[i].t, args[i]);
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
            if (i < 0 || (size_t)i >= xs.size()) MINIS_ERR("{V5}", *src, p.i, "");
            push(xs[(size_t)i]);
          } else if (base.t == Type::Str) {
            auto& s = std::get<std::string>(base.v);
            if (i < 0 || (size_t)i >= s.size()) MINIS_ERR("{V5}", *src, p.i, "");
            push(Value::S(std::string(1, s[(size_t)i])));
          } else {
            MINIS_ERR("{V4}", *src, p.i, std::string("expected listt/string, got ") + TypeName(base.t));
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
          MINIS_ERR("{V5}", *src, p.i, "bad opcode");
      } // switch
    } // for
  } // run()
};

/*=============================
=        Public API           =
=============================*/
inline std::string read_file(const std::string& path){
  std::ifstream in(path, std::ios::binary);
  if(!in) MINIS_ERR("{T5}", *src, p.i, "cannot open "+path);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

// Name generator for minification
static std::string gensym_name(uint64_t n) {
  uint64_t remaining = n;
  uint64_t len = 1;
  uint64_t block = 26;
  while (remaining >= block) { remaining -= block; ++len; block *= 26ULL; }

  std::string out;
  out.reserve(len);
  for (uint64_t i = 0; i < len; ++i) {
    uint64_t d = remaining % 26ULL;
    out.push_back(char('a' + d));
    remaining /= 26ULL;
  }
  return out;
}

// Token types for lexer
enum TokKind { T_Id, T_Num, T_Str, T_Sym, T_WS, T_EOF };
struct Tok { TokKind k; std::string text; size_t pos; };

static bool isIdStart(char c){ return std::isalpha((unsigned char)c) || c=='_'; }
static bool isIdCont(char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='.'; }

// Lexer implementation
static std::vector<Tok> lex_minis(const std::string& src){
  std::vector<Tok> ts;
  ts.reserve(src.size()/3);
  size_t i=0, n=src.size();
  auto push=[&](TokKind k, size_t s){ ts.push_back({k, src.substr(s, i-s), s}); };

  while(i<n){
    size_t s=i;
    if(std::isspace((unsigned char)src[i])){
      while(i<n && std::isspace((unsigned char)src[i])) ++i;
      push(T_WS,s);
      continue;
    }
    if(i+1<n && src[i]=='/' && src[i+1]=='/'){
      i+=2;
      while(i<n && src[i]!='\n') ++i;
      continue;
    }
    if(i+1<n && src[i]=='/' && src[i+1]=='*'){
      i+=2; int depth=1;
      while(i+1<n && depth>0){
        if(src[i]=='/' && src[i+1]=='*'){ depth++; i+=2; }
        else if(src[i]=='*' && src[i+1]=='/'){ depth--; i+=2; }
        else ++i;
      }
      continue;
    }
    if(src[i]=='"' || src[i]=='\''){
      char q=src[i++]; bool esc=false;
      while(i<n){
        char c=src[i++];
        if(esc){ esc=false; continue; }
        if(c=='\\'){ esc=true; continue; }
        if(c==q) break;
      }
      push(T_Str,s);
      continue;
    }
    if(std::isdigit((unsigned char)src[i]) || ((src[i]=='+'||src[i]=='-') && i+1<n && std::isdigit((unsigned char)src[i+1]))){
      ++i;
      while(i<n && (std::isdigit((unsigned char)src[i]) || src[i]=='.')) ++i;
      push(T_Num,s);
      continue;
    }
    if(isIdStart(src[i])){
      ++i;
      while(i<n && isIdCont(src[i])) ++i;
      push(T_Id,s);
      continue;
    }
    ++i;
    push(T_Sym,s);
  }
  ts.push_back({T_EOF,"",n});
  return ts;
}

// Keyword and builtin checking
namespace {
const std::unordered_set<std::string> kw={
    "func","let","if","elif","else","while","return","break","continue",
    "true","false","null","auto","int","float","bool","str","list",
    "conv","del","exit","try","except","finally","lambda","with","and",
    "inline","tailcall","void","yield","owned","shared","static","const"
};
}

static bool is_kw(const std::string& s){
  return kw.count(s)!=0;
}

static bool is_builtin(const std::string& s){
  static const std::unordered_set<std::string> bi={
    "print","abs","neg","range","len","input","max","min","sort","reverse","sum"
  };
  return bi.count(s)!=0;
}

// Rename planning
struct RenamePlan{
  std::unordered_map<std::string,std::string> id2mini;
  uint64_t counter=0;

  const std::string& ensure(const std::string& name){
    auto it=id2mini.find(name);
    if(it!=id2mini.end()) return it->second;
    auto alias = gensym_name(counter++);
    auto [jt,_] = id2mini.emplace(name, std::move(alias));
    return jt->second;
  }
};

static RenamePlan plan_renames(const std::vector<Tok>& ts){
  RenamePlan plan;
  for(size_t i=0;i+1<ts.size();++i){
    if(ts[i].k==T_Id && ts[i].text=="func"){
      size_t j=i+1;
      if(ts[j].k==T_Id && (ts[j].text=="void"||ts[j].text=="int"||ts[j].text=="float"||
                           ts[j].text=="bool"||ts[j].text=="str"||ts[j].text=="list")) { ++j; }
      while(ts[j].k==T_WS) ++j;
      if(ts[j].k==T_Id && !is_kw(ts[j].text) && !is_builtin(ts[j].text)){
        plan.ensure(ts[j].text);
      }
    }
    if(ts[i].k==T_Id && ts[i].text=="let"){
      size_t j=i+1;
      while(ts[j].k==T_Id && (ts[j].text=="const"||ts[j].text=="static"||ts[j].text=="owned"||ts[j].text=="shared")) ++j;
      while(ts[j].k==T_WS) ++j;
      if(ts[j].k==T_Id && (ts[j].text=="auto"||ts[j].text=="null"||ts[j].text=="int"||ts[j].text=="float"||
                           ts[j].text=="bool"||ts[j].text=="str"||ts[j].text=="list")) ++j;
      while(ts[j].k==T_WS) ++j;
      if(ts[j].k==T_Id && !is_kw(ts[j].text) && !is_builtin(ts[j].text)){
        plan.ensure(ts[j].text);
      }
    }
  }
  return plan;
}

// Rebuild minified source
static std::string rebuild_minified(const std::vector<Tok>& ts, const RenamePlan& plan){
  auto need_space = [](const Tok& a, const Tok& b)->bool{
    auto idlike = [](TokKind k){ return k==T_Id || k==T_Num; };
    return idlike(a.k) && idlike(b.k);
  };
  std::string out;
  out.reserve(ts.size()*4);
  Tok prev{T_Sym,"",0};

  for(size_t i=0;i<ts.size();++i){
    const Tok& t = ts[i];
    if(t.k==T_EOF) break;
    if(t.k==T_WS) continue;

    std::string chunk;
    switch(t.k){
      case T_Id:{
        if(!is_kw(t.text) && !is_builtin(t.text)){
          auto it = plan.id2mini.find(t.text);
          chunk = (it!=plan.id2mini.end()) ? it->second : t.text;
        } else chunk = t.text;
      } break;
      case T_Str:
      case T_Num:
      case T_Sym: chunk = t.text; break;
      default: break;
    }

    if(!out.empty() && need_space(prev, t)) out.push_back(' ');
    out += chunk;
    prev = t;
  }
  return out;
}

// Preprocessing and minification
struct PreprocResult {
  std::string out;
  std::vector<size_t> posmap;   // out[i] -> raw offset
};

// NOTE: we keep logic same but when we copy any byte into `out`, we push raw index into posmap
static PreprocResult preprocess_and_minify_with_map(const std::string& raw) {
  auto toks = lex_minis(raw);   // you already have this
  auto plan = plan_renames(toks);

  // Rebuild with spacing logic, but track positions.
  auto need_space = [](const Tok& a, const Tok& b)->bool{
    auto idlike = [](TokKind k){ return k==T_Id || k==T_Num; };
    return idlike(a.k) && idlike(b.k);
  };

  std::string out;
  std::vector<size_t> posmap;
  out.reserve(raw.size()/2);
  posmap.reserve(raw.size()/2);

  Tok prev{T_Sym,"",0};
  for (size_t i=0; i<toks.size(); ++i) {
    const Tok& t = toks[i];
    if (t.k == T_EOF) break;
    if (t.k == T_WS) continue;

    std::string chunk;
    switch (t.k) {
      case T_Id: {
        if (!is_kw(t.text) && !is_builtin(t.text)) {
          auto it = plan.id2mini.find(t.text);
          chunk = (it!=plan.id2mini.end()) ? it->second : t.text;
        } else chunk = t.text;
      } break;
      case T_Str:
      case T_Num:
      case T_Sym:
        chunk = t.text; break;
      default: break;
    }

    // space if needed
    if (!out.empty() && need_space(prev, t)) {
      out.push_back(' ');
      // best effort: map space to the start of next token in raw
      posmap.push_back(t.pos);
    }

    // copy chunk and attach raw positions
    for (size_t k = 0; k < chunk.size(); ++k) {
      out.push_back(chunk[k]);
      // Choose a stable original index for this produced character.
      // For ids/strings/syms/nums we map to the token start + min(k, token_len-1)
      posmap.push_back(t.pos + std::min(k, t.text.size() ? t.text.size()-1 : 0));
    }

    prev = t;
  }
  return { std::move(out), std::move(posmap) };
}


inline void compile_file_to_avocado(const std::string& srcName, const std::string& srcText, const std::string& outAvo){
  Source S{srcName, srcText};
  Compiler C(S);
  C.compileToFile(outAvo);
}

inline void run_avocado(const std::string& bcPath){
  VM vm;
  auto& builtins = minis::builtins;
  for (const auto& [name, fn] : builtins) {
    vm.globals.Declare(name, Type::Null, Value::N());
  }
  vm.load(bcPath);
  vm.run();
}
}; // namespace minis

int main(int argc, char** argv){
  try {
    std::string inputPath;
    std::string outPath = "a.mi";
    bool debug = false;
    bool preprocessOnly = false;
    int optLevel = 0;

    // very small arg parser
    for (int i=1; i<argc; ++i) {
      std::string a = argv[i];
      if (a == "-o") {
        if (i+1 >= argc) {
          std::cerr << "Error: -o requires an output file path\n";
          return 2;
        }
        outPath = argv[++i];
      }
      else if (a == "-d" || a == "-debug"){ debug = true; }
      else if (a == "-E")               { preprocessOnly = true; }
      else if (a == "-O0")              { optLevel = 0; }
      else if (a == "-O1")              { optLevel = 1; }
      else if (!a.empty() && a[0] == '-') {
        std::cerr << "Unknown flag: " << a << "\n";
        return 2;
      } else {
        inputPath = a;
      }
    }

    if (inputPath.empty()) {
      std::cerr << "Usage: cmin [-d|-debug] [-O0|-O1] [-E] <input.minis> -o <out.mi>\n";
      return 2;
    }

    std::string raw = minis::read_file(inputPath);

    std::string compileBuf;
    if (debug) {
      // no preprocess: identity map
      minis::g_posmap.resize(raw.size());
      for (size_t i=0;i<raw.size();++i) minis::g_posmap[i] = i;
      compileBuf = raw;
    } else {
      auto prep = minis::preprocess_and_minify_with_map(raw);
      minis::g_posmap = std::move(prep.posmap);
      compileBuf = std::move(prep.out);
    }

    if (preprocessOnly) {
      std::cout << compileBuf;
      return 0;
    }

    // IMPORTANT: point minis::src at the ORIGINAL for error printing
    Source S{ inputPath, raw };           // expects err.hpp defines Source{name,text}
    minis::Compiler C(S);
    // force parser to use the cleaned buffer
    C.P.src = &compileBuf;

    C.compileToFile(outPath);
    minis::run_avocado(outPath);
    return 0;

  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
