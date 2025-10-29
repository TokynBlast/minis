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
