#ifndef MINIS_HPP_INCLUDED
#define MINIS_HPP_INCLUDED
#pragma once

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
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <memory>


#ifdef _WIN32
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
#endif
#include <functional>

namespace minis {

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
struct Source{ std::string name, text; };
struct Pos{ size_t i=0; const std::string* src=nullptr; };

inline bool AtEnd(Pos&p){ return p.i>=p.src->size(); }
inline bool IsIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
inline bool IsIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

// Replace your current SkipWS with this nested-aware version.
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

    // 3) /* block comment */  (supports nesting)
    if (p.i + 1 < s.size() && s[p.i] == '/' && s[p.i + 1] == '*') {
      p.i += 2;
      int depth = 1;
      while (p.i + 1 < s.size() && depth > 0) {
        if (s[p.i] == '/' && s[p.i + 1] == '*') { depth++; p.i += 2; continue; }
        if (s[p.i] == '*' && s[p.i + 1] == '/') { depth--; p.i += 2; continue; }
        ++p.i;
      }
      // If file ended mid-comment, we just stop; later code will report cleanly.
      continue;
    }

    break; // nothing else to skip
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
inline bool Match(Pos&p,char c){ SkipWS(p); if(!AtEnd(p)&&(*p.src)[p.i]==c){++p.i; return true;} return false; }
inline bool MatchStr(Pos&p,const char*s){ SkipWS(p); size_t L=strlen(s); if(p.i+L<=p.src->size() && p.src->compare(p.i,L,s)==0){ p.i+=L; return true;} return false; }
inline void Expect(Pos&p,char c){
  SkipWS(p); size_t w=p.i; if(AtEnd(p)||(*p.src)[p.i]!=c) throw ScriptError(std::string("expected '")+c+"'", Span{w,w+1}); ++p.i;
}
inline std::string ParseIdent(Pos&p){
  SkipWS(p); size_t s=p.i; if(AtEnd(p)||!IsIdStart((*p.src)[p.i])) throw ScriptError("expected identifier", Span{s,s+1});
  ++p.i; while(!AtEnd(p)&&IsIdCont((*p.src)[p.i])) ++p.i; return p.src->substr(s,p.i-s);
}
inline std::string ParseQuoted(Pos&p){
  SkipWS(p); if(AtEnd(p)) throw ScriptError("expected string", Span{p.i,p.i});
  char q=(*p.src)[p.i]; if(q!='"'&&q!='\'') throw ScriptError("expected string", Span{p.i,p.i+1});
  ++p.i; std::string out;
  while(!AtEnd(p)&&(*p.src)[p.i]!=q){
    char c=(*p.src)[p.i++]; if(c=='\\'){ if(AtEnd(p)) throw ScriptError("unterminated string",{}); char n=(*p.src)[p.i++];
      switch(n){ case'n':out.push_back('\n');break; case't':out.push_back('\t');break; case'r':out.push_back('\r');break;
        case'\\':out.push_back('\\');break; case'"':out.push_back('"');break; case'\'':out.push_back('\'');break;
        default: out.push_back(n); break; }
    } else out.push_back(c);
  }
  if(AtEnd(p)||(*p.src)[p.i]!=q) throw ScriptError("unterminated string",{}); ++p.i; return out;
}
inline std::string ParseNumberText(Pos&p){
  SkipWS(p); size_t s=p.i; if(!AtEnd(p)&&(((*p.src)[p.i]=='+')||((*p.src)[p.i]=='-'))) ++p.i;
  bool dig=false,dot=false; while(!AtEnd(p)){ char c=(*p.src)[p.i];
    if(std::isdigit((unsigned char)c)){dig=true;++p.i;} else if(c=='.'&&!dot){dot=true;++p.i;} else break;}
  if(!dig) throw ScriptError("expected number", Span{s,s+1}); return p.src->substr(s,p.i-s);
}

/*=============================
=          Values/Env         =
=============================*/
enum class Type{ Int=0, Float=1, Bool=2, Str=3, List=4 };
struct Value{
  Type t;
  std::variant<long long,double,bool,std::string,std::vector<Value>> v;
  static Value I(long long x){ return {Type::Int,x}; }
  static Value F(double x){ return {Type::Float,x}; }
  static Value B(bool x){ return {Type::Bool,x}; }
  static Value S(std::string s){ return {Type::Str,std::move(s)}; }
  static Value L(std::vector<Value> xs){ return {Type::List,std::move(xs)}; }

  long long AsInt()const{ switch(t){case Type::Int:return std::get<long long>(v);case Type::Float:return (long long)std::get<double>(v);
    case Type::Bool:return std::get<bool>(v)?1:0; default: throw ScriptError("cannot convert to int");}}
  double AsFloat()const{ switch(t){case Type::Int:return (double)std::get<long long>(v);case Type::Float:return std::get<double>(v);
    case Type::Bool:return std::get<bool>(v)?1.0:0.0; default: throw ScriptError("cannot convert to float");}}
  bool AsBool()const{ switch(t){case Type::Bool:return std::get<bool>(v);case Type::Int:return std::get<long long>(v)!=0;
    case Type::Float:return std::get<double>(v)!=0.0; case Type::Str:return !std::get<std::string>(v).empty();
    case Type::List:return !std::get<std::vector<Value>>(v).empty();} return false;}
  std::string AsStr()const{
    switch(t){case Type::Str:return std::get<std::string>(v);
      case Type::Int:return std::to_string(std::get<long long>(v));
      case Type::Float:{ std::ostringstream os; os<<std::get<double>(v); return os.str(); }
      case Type::Bool:return std::get<bool>(v)?"true":"false";
      case Type::List:{ const auto&xs=std::get<std::vector<Value>>(v); std::ostringstream os; os<<"["; for(size_t i=0;i<xs.size();++i){ if(i) os<<","; os<<xs[i].AsStr(); } os<<"]"; return os.str();}}
    return{};}
  const std::vector<Value>& AsList()const{ if(t!=Type::List) throw ScriptError("expected list"); return std::get<std::vector<Value>>(v);}
  std::vector<Value>& AsList(){ if(t!=Type::List) throw ScriptError("expected list"); return std::get<std::vector<Value>>(v);}
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

struct Var{ Type declared; Value val; };
struct Env{
  std::unordered_map<std::string,Var> m; Env* parent=nullptr;
  Env(Env*p=nullptr):parent(p){}
  bool ExistsLocal(const std::string&n)const{ return m.find(n)!=m.end();}
  bool Exists(const std::string&n)const{ return ExistsLocal(n)||(parent && parent->Exists(n));}
  const Var& Get(const std::string&n)const{
    auto it=m.find(n); if(it!=m.end()) return it->second; if(parent) return parent->Get(n);
    throw ScriptError("unknown variable: "+n);
  }
  void Declare(const std::string&n,Type t,Value v){
    if(m.count(n)) throw ScriptError("already declared: "+n);
    Coerce(t,v); m.emplace(n,Var{t,v});
  }
  void Set(const std::string&n,Value v){
    auto it=m.find(n); if(it!=m.end()){ Coerce(it->second.declared,v); it->second.val=v; return; }
    if(parent){ parent->Set(n,v); return; } throw ScriptError("unknown variable: "+n);
  }
  void SetOrDeclare(const std::string&n,Value v){ if(ExistsLocal(n)) Set(n,v); else if(parent&&parent->Exists(n)) parent->Set(n,v); else Declare(n,v.t,v); }
  bool Erase(const std::string&n){ return m.erase(n)!=0; }
  inline const char* TypeName(Type t){
    switch(t){
      case Type::Int:  return "int";
      case Type::Float:return "float";
      case Type::Bool: return "bool";
      case Type::Str:  return "str";
      case Type::List: return "list";
    }
    return "?";
  }

  bool Unset(const std::string& n){
    if (Erase(n)) return true;
    return parent ? parent->Unset(n) : false;
  }

  void Coerce(Type t, Value& v){
    if (v.t == t) return;
    switch (t){
      case Type::Int:   v = Value::I(v.AsInt());   break;
      case Type::Float: v = Value::F(v.AsFloat()); break;
      case Type::Bool:  v = Value::B(v.AsBool());  break;
      case Type::List:
        if (v.t != Type::List) throw ScriptError(std::string("cannot assign ") + TypeName(v.t) + " to list");
        break;
      case Type::Str:
        if (v.t != Type::Str)  throw ScriptError(std::string("cannot assign ") + TypeName(v.t) + " to str");
        break;
    }
  }
};

/*=============================
=           Builtins          =
=============================*/
struct Builtins{
  std::unordered_map<std::string, std::function<std::optional<Value>(const std::vector<Value>&)>> f;
#ifdef _WIN32
  DWORD oldIn=0, oldOut=0;
#else
  termios oldT{}; bool haveOld=false;
#endif

  Builtins(){ Install(); }
  ~Builtins(){ leaveRaw(); }

  inline void addVoid(const std::string&n,std::function<void(const std::vector<Value>&)> fn){ f[n]=[fn](auto& a)->std::optional<Value>{ fn(a); return std::nullopt;}; }
  inline void addRet (const std::string&n,std::function<Value(const std::vector<Value>&)> fn){ f[n]=[fn](auto& a)->std::optional<Value>{ return fn(a); }; }
  static inline void req(const std::vector<Value>&a,size_t n,const char*name){ if(a.size()!=n){ std::ostringstream os; os<<name<<" expects "<<n<<" args"; throw ScriptError(os.str()); } }

  inline void Install(){
    addVoid("cls",[&](auto&){ std::cout<<"\x1b[2J\x1b[H"; std::cout.flush(); });
    addVoid("pos",[&](auto&a){ req(a,2,"pos"); std::cout<<"\x1b["<<a[1].AsInt()<<";"<<a[0].AsInt()<<"H"; std::cout.flush(); });
    addVoid("print",[&](auto&a){ req(a,1,"print"); std::string s=a[0].AsStr(); for(char&ch:s) if(ch=='\t') ch=' '; std::cout<<s; });

    addRet ("len",[&](auto&a){ req(a,1,"len"); if(a[0].t==Type::Str) return Value::I((long long)a[0].AsStr().size());
                                if(a[0].t==Type::List) return Value::I((long long)a[0].AsList().size());
                                throw ScriptError("len expects str/list"); });

    addRet("Load",[&](auto&a){ req(a,1,"Load"); std::ifstream in(a[0].AsStr(),std::ios::binary); if(!in) return Value::S(""); std::string d((std::istreambuf_iterator<char>(in)),{}); return Value::S(d);});
    addRet("Save",[&](auto&a){ req(a,2,"Save"); std::ofstream out(a[0].AsStr(),std::ios::binary|std::ios::trunc); if(!out) return Value::B(false); auto s=a[1].AsStr(); out.write(s.data(),(std::streamsize)s.size()); return Value::B((bool)out);});

    addRet ("ListGet",[&](auto&a){ req(a,2,"ListGet"); if(a[0].t!=Type::List) throw ScriptError("ListGet expects list"); auto&xs=const_cast<std::vector<Value>&>(a[0].AsList()); size_t i=(size_t)a[1].AsInt(); if(i>=xs.size()) return Value::I(0); return xs[i];});
    addVoid("ListSet",[&](auto&a){ req(a,3,"ListSet"); if(a[0].t!=Type::List) throw ScriptError("ListSet expects list"); auto&xs=const_cast<std::vector<Value>&>(a[0].AsList()); size_t i=(size_t)a[1].AsInt(); if(i>=xs.size()) throw ScriptError("ListSet: OOB"); xs[i]=a[2];});
    addVoid("ListPush",[&](auto&a){ req(a,2,"ListPush"); if(a[0].t!=Type::List) throw ScriptError("ListPush expects list"); auto&xs=const_cast<std::vector<Value>&>(a[0].AsList()); xs.push_back(a[1]);});

    addVoid("Input.Start",[&](auto&){ enterRaw(); });
    addVoid("Input.Stop",[&](auto&){ leaveRaw(); });
    addVoid("Input.EnableMouse",[&](auto&){ std::cout<<"\x1b[?1000h\x1b[?1006h"; std::cout.flush(); });
    addVoid("Input.DisableMouse",[&](auto&){ std::cout<<"\x1b[?1000l\x1b[?1006l"; std::cout.flush(); });
    addRet ("Input.Key",[&](auto&){ if(!std::cin.good()) return Value::S(""); int c=std::cin.get(); if(c==EOF) return Value::S("");
      if(c=='\r'||c=='\n'){ if(c=='\r'&&std::cin.rdbuf()->in_avail()){int pk=std::cin.peek(); if(pk=='\n')(void)std::cin.get();} return Value::S("Enter");}
      if(c==0x08 || c==0x7F) return Value::S("BackSpace");
      if(c==0x1B){ std::string s; s.push_back((char)0x1B); std::this_thread::sleep_for(std::chrono::milliseconds(2));
        while(std::cin.rdbuf()->in_avail()){ int d=std::cin.get(); if(d==EOF) break; s.push_back((char)d); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        if(s.size()==1) return Value::S("Escape"); return Value::S(s);}
      std::string s; s.push_back((char)c); return Value::S(s); });

    addRet("Input.Mouse", [&](const std::vector<Value>& a) -> Value {
      req(a, 0, "Input.Mouse");

      auto emptyList = []() -> Value {
        std::vector<minis::Value> v;               // avoid {} initializer pitfalls
        return minis::Value::L(std::move(v));
      };

      // Nothing to read? => []
      if (!std::cin.good() || std::cin.rdbuf()->in_avail() <= 0)
        return emptyList();

      // Must start with ESC
      int c1 = std::cin.peek();
      if (c1 != 0x1B) return emptyList();
      std::cin.get(); // ESC

      // Expect '['
      if (std::cin.rdbuf()->in_avail() <= 0) { std::cin.putback((char)0x1B); return emptyList(); }
      if (std::cin.peek() != '[') { std::cin.putback((char)0x1B); return emptyList(); }
      std::cin.get(); // '['

      // Expect '<'
      if (std::cin.rdbuf()->in_avail() <= 0) { std::cin.putback('['); std::cin.putback((char)0x1B); return emptyList(); }
      if (std::cin.peek() != '<') { std::cin.putback('['); std::cin.putback((char)0x1B); return emptyList(); }
      std::cin.get(); // '<'

      auto readInt = [] (int& out) -> bool {
        std::string num;
        while (std::cin.good() && std::cin.rdbuf()->in_avail() > 0) {
          int p = std::cin.peek();
          if (p < 0 || !std::isdigit((unsigned char)p)) break;
          num.push_back((char)std::cin.get());
        }
        if (num.empty()) return false;
        try { out = std::stoi(num); } catch (...) { return false; }
        return true;
      };

      int b=0, x=0, y=0;

      if (!readInt(b)) return emptyList();
      if (std::cin.rdbuf()->in_avail() <= 0 || std::cin.peek() != ';') return emptyList();
      std::cin.get(); // ';'

      if (!readInt(x)) return emptyList();
      if (std::cin.rdbuf()->in_avail() <= 0 || std::cin.peek() != ';') return emptyList();
      std::cin.get(); // ';'

      if (!readInt(y)) return emptyList();
      if (std::cin.rdbuf()->in_avail() <= 0) return emptyList();

      int term = std::cin.get(); // 'M' (press/drag) or 'm' (release)
      if (term != 'M' && term != 'm') return emptyList();

      // classify kind + button
      std::string kind;
      if (b & 64) {                         // wheel (64..67)
        kind = (b & 1) ? "wheel-down" : "wheel-up";
      } else if (b & 32) {                  // motion/drag
        kind = "drag";
      } else {                              // button press/release
        kind = (term == 'M') ? "down" : "up";
      }
      int button = (b & 3) + 1;             // 1..3

      std::vector<minis::Value> res;
      res.emplace_back(minis::Value::S(kind));
      res.emplace_back(minis::Value::I(button));
      res.emplace_back(minis::Value::I(x));
      res.emplace_back(minis::Value::I(y));
      return minis::Value::L(std::move(res));
    });

  };

  inline void enterRaw(){
#ifdef _WIN32
    HANDLE hin=GetStdHandle(STD_INPUT_HANDLE), hout=GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hin,&oldIn); GetConsoleMode(hout,&oldOut);
    DWORD inM=oldIn; inM&=~(ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT); inM|=ENABLE_VIRTUAL_TERMINAL_INPUT; SetConsoleMode(hin,inM);
    DWORD outM=oldOut; outM|=ENABLE_VIRTUAL_TERMINAL_PROCESSING; SetConsoleMode(hout,outM);
#else
    termios t{}; if(tcgetattr(STDIN_FILENO,&oldT)==0){ haveOld=true; t=oldT; cfmakeraw(&t); tcsetattr(STDIN_FILENO,TCSANOW,&t); }
#endif
  }
  inline void leaveRaw(){
#ifdef _WIN32
    HANDLE hin=GetStdHandle(STD_INPUT_HANDLE), hout=GetStdHandle(STD_OUTPUT_HANDLE);
    if(oldIn) SetConsoleMode(hin,oldIn); if(oldOut) SetConsoleMode(hout,oldOut);
#else
    if(haveOld) tcsetattr(STDIN_FILENO,TCSANOW,&oldT);
#endif
  }
};

/*=============================
=           Bytecode          =
=============================*/
enum Op : uint8_t {
  OP_NOP, OP_PUSH_I, OP_PUSH_F, OP_PUSH_B, OP_PUSH_S, OP_MAKE_LIST,
  OP_GET, OP_SET, OP_DECL, OP_POP,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV, // OP_MOD,
  OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
  OP_AND, OP_OR,
  OP_JMP, OP_JF, OP_CALL_BUILTIN, OP_CALL_USER,
  OP_MOUSE,
  OP_RET, OP_RET_VOID, OP_HALT,
  OP_UNSET
  /*
  // Implement later
  OP_UP // Add one to the current var
  OP_DOWN // Subtract one from the current var
  OP_IMPORT // Import a script's functions and variables
  OP_CLONE // Clone a var into the latter, without removing the former
  */
};
struct FnInfo{
  std::string name;
  uint64_t entry=0;
  std::vector<std::string> params;
  bool isVoid=false;
  bool typed=false; // return type explicitly declared (non-void)
  Type ret=Type::Int;
};

// IO helpers (inline static to avoid ODR)
inline static void write_u8 (FILE*f, uint8_t  v){ fwrite(&v,1,1,f); }
inline static void write_u64(FILE*f, uint64_t v){ fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64_t  v){ fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const std::string&s){ uint64_t n=s.size(); write_u64(f,n); if(n) fwrite(s.data(),1,n,f); }

inline static uint8_t  read_u8 (FILE*f){ uint8_t v;  fread(&v,1,1,f); return v; }
inline static uint64_t read_u64(FILE*f){ uint64_t v; fread(&v,8,1,f); return v; }
inline static int64_t  read_s64(FILE*f){ int64_t v;  fread(&v,8,1,f); return v; }
inline static double   read_f64(FILE*f){ double v;   fread(&v,8,1,f); return v; }
inline static std::string read_str(FILE*f){ uint64_t n=read_u64(f); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); return s; }

/*=============================
=           Compiler          =
=============================*/
struct Compiler {
  Source src; Pos P; Builtins* builtins;
  FILE* out=nullptr;
  std::vector<FnInfo> fns;
  std::unordered_map<std::string,size_t> fnIndex;

  // Avocado header fields
  uint64_t table_offset_pos=0, fn_count_pos=0, entry_main_pos=0;

  explicit Compiler(const Source&s, Builtins*b):src(s),builtins(b){ P.src=&src.text; }

  inline Type parseType(){
    if(StartsWithKW(P,"int")){ P.i+=3; return Type::Int;}
    if(StartsWithKW(P,"float")){ P.i+=5; return Type::Float;}
    if(StartsWithKW(P,"bool")){ P.i+=4; return Type::Bool;}
    if(StartsWithKW(P,"str")){ P.i+=3; return Type::Str;}
    if(StartsWithKW(P,"list")){ P.i+=4; return Type::List;}
    throw ScriptError("unknown type (use int|float|bool|str|list)");
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
  inline void LogicOr(){ LogicAnd(); while(MatchStr(P,"||")){ LogicAnd(); emit_u8(OP_OR); } }
  inline void LogicAnd(){ Equality(); while(MatchStr(P,"&&")){ Equality(); emit_u8(OP_AND); } }
  inline void Equality(){
    AddSub();
    while(true){
      if(MatchStr(P,"==")){ AddSub(); emit_u8(OP_EQ);}
      else if(MatchStr(P,"!=")){ AddSub(); emit_u8(OP_NE);}
      else if(MatchStr(P,">=")){ AddSub(); emit_u8(OP_GE);}
      else if(MatchStr(P,">")) { AddSub(); emit_u8(OP_GT);}
      else if(MatchStr(P,"<=")){ AddSub(); emit_u8(OP_LE);}
      else if(MatchStr(P,"<")) { AddSub(); emit_u8(OP_LT);}
      else break;
    }
  }
  inline void AddSub(){
    MulDiv();
    while(true){
      if(Match(P,'+')){ MulDiv(); emit_u8(OP_ADD); }
      else if(Match(P,'-')){ MulDiv(); emit_u8(OP_SUB); }
      else break;
    }
  }
  inline void MulDiv(){
    Factor();
    while(true){
      if(Match(P,'*')){ Factor(); emit_u8(OP_MUL); }
      else if(Match(P,'/')){ Factor(); emit_u8(OP_DIV); }
      else break;
    }
  }
  inline void ListLit(){
    size_t count=0;
    if(Match(P,']')){ emit_u8(OP_MAKE_LIST); emit_u64(0); return; }
    while(true){ Expr(); ++count; SkipWS(P); if(Match(P,']')) break; Expect(P,','); }
    emit_u8(OP_MAKE_LIST); emit_u64(count);
  }
  inline void Factor(){
    SkipWS(P);
    if(!AtEnd(P)&&(*P.src)[P.i]=='('){ ++P.i; Expr(); Expect(P,')'); return; }
    if(!AtEnd(P) && ((*P.src)[P.i]=='"'||(*P.src)[P.i]=='\'')){ auto s=ParseQuoted(P); emit_u8(OP_PUSH_S); emit_str(s); return; }
    if(P.i+4<=P.src->size() && P.src->compare(P.i,4,"true")==0 && (P.i+4==P.src->size()||!IsIdCont((*P.src)[P.i+4]))){ P.i+=4; emit_u8(OP_PUSH_B); emit_u8(1); return; }
    if(P.i+5<=P.src->size() && P.src->compare(P.i,5,"false")==0&& (P.i+5==P.src->size()||!IsIdCont((*P.src)[P.i+5]))){ P.i+=5; emit_u8(OP_PUSH_B); emit_u8(0); return; }
    if(!AtEnd(P)&&(*P.src)[P.i]=='['){ ++P.i; ListLit(); return; }
    if(!AtEnd(P)&&(std::isdigit((unsigned char)(*P.src)[P.i])||(*P.src)[P.i]=='+'||(*P.src)[P.i]=='-')){
      auto s=ParseNumberText(P); if(s.find('.')!=std::string::npos){ emit_u8(OP_PUSH_F); emit_f64(std::stod(s)); }
      else{ emit_u8(OP_PUSH_I); emit_s64(std::stoll(s)); } return;
    }
    if(!AtEnd(P)&&IsIdStart((*P.src)[P.i])){
      auto id=ParseIdent(P); SkipWS(P);
      if(!AtEnd(P)&&(*P.src)[P.i]=='('){ // call
        ++P.i; size_t argc=0;
        if(!Match(P,')')){ while(true){ Expr(); ++argc; SkipWS(P); if(Match(P,')')) break; Expect(P,','); } }
        if(builtins->f.count(id)){ emit_u8(OP_CALL_BUILTIN); emit_str(id); emit_u64(argc); }
        else{ emit_u8(OP_CALL_USER); emit_str(id); emit_u64(argc); }
        return;
      } else { emit_u8(OP_GET); emit_str(id); return; }
    }
    throw ScriptError("unexpected token in expression", Span{P.i,P.i+1});
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
      if(StartsWithKW(P,"exit")){ P.i+=4; Expect(P,';'); emit_u8(OP_HALT); continue; }

      if(StartsWithKW(P,"import")){ P.i+=6; SkipWS(P);
        if(!AtEnd(P)&&((*P.src)[P.i]=='"'||(*P.src)[P.i]=='\'')) (void)ParseQuoted(P); else (void)ParseIdent(P);
        Expect(P,';'); continue; }

      if(StartsWithKW(P,"del")){ P.i+=3; SkipWS(P); auto n=ParseIdent(P); Expect(P,';');
        emit_u8(OP_UNSET); emit_str(n); continue; }

      if(StartsWithKW(P,"return")){
        P.i+=6; SkipWS(P);
        if(Match(P,';')){ emit_u8(OP_RET_VOID); continue; }
        Expr(); Expect(P,';'); emit_u8(OP_RET); continue;
      }

      if(StartsWithKW(P,"continue")){
        P.i+=8; SkipWS(P); Expect(P,';');
        if(loopStack.empty()) throw ScriptError("'continue' outside loop");
        emit_u8(OP_JMP); emit_u64(loopStack.back().contTarget);
        continue;
      }

      if(StartsWithKW(P,"break")){
        P.i+=5; size_t levels=1; SkipWS(P);
        if(!AtEnd(P) && std::isdigit((unsigned char)(*P.src)[P.i])){ auto num=ParseNumberText(P); levels=(size_t)std::stoll(num); }
        Expect(P,';');
        if(loopStack.size()<levels) throw ScriptError("'break' outside loop");
        size_t idx=loopStack.size()-levels;
        emit_u8(OP_JMP); auto at=tell(); emit_u64(0);
        loopStack[idx].breakPatchSites.push_back(at);
        continue;
      }

      // --- func (typed return supported; params are names only) -------------------
      if (StartsWithKW(P, "func")) {
        P.i += 4; SkipWS(P);

        bool isVoid = false, typed = false;
        Type rt = Type::Int;
        std::string fname;

        // Return type (optional): void | int | float | bool | str | list
        Pos look = P;
        if (StartsWithKW(look,"void") || StartsWithKW(look,"int") || StartsWithKW(look,"float") ||
            StartsWithKW(look,"bool") || StartsWithKW(look,"str") || StartsWithKW(look,"list")) {
          if (StartsWithKW(P,"void")) { P.i += 4; isVoid = true; }
          else { rt = parseType(); typed = true; }
          SkipWS(P);
          fname = ParseIdent(P);
        } else {
          fname = ParseIdent(P);
        }

        SkipWS(P); Expect(P,'(');
        std::vector<std::string> params;
        SkipWS(P);
        if (!Match(P,')')) {
          while (true) {
            params.push_back(ParseIdent(P));
            SkipWS(P);
            if (Match(P,')')) break;
            Expect(P, ',');
          }
        }
        SkipWS(P); Expect(P,'{');

        // Register function
        FnInfo fni{fname, 0, params, isVoid, typed, rt};
        size_t idx = fns.size();
        fns.push_back(fni);
        fnIndex[fname] = idx;

        // IMPORTANT: skip over the function body in __main__
        emit_u8(OP_JMP);
        auto skipAt = tell();          // placeholder target for after-body
        emit_u64(0);

        // Function entry begins HERE (not at 'func' token)
        auto entry = tell();
        fns[idx].entry = entry;

        // Compile the body (balanced braces)
        size_t depth = 1;
        while (!AtEnd(P)) {
          if ((*P.src)[P.i] == '{') { ++depth; ++P.i; continue; }
          if ((*P.src)[P.i] == '}') { --depth; ++P.i; if (depth == 0) break; continue; }
          StmtSeqOne();
        }

        // Ensure a return opcode at the end of body
        if (isVoid) emit_u8(OP_RET_VOID);
        else        emit_u8(OP_RET);

        // Patch the top-level skip to here (after the body)
        auto after = tell();
        patchJump(skipAt, after);
        continue;
      }


      // while (cond) { ... }
      if(StartsWithKW(P,"while")){
        P.i+=5; SkipWS(P); Expect(P,'(');
        auto condOff = tell();
        Expr(); Expect(P,')');
        emit_u8(OP_JF); auto jfAt = tell(); emit_u64(0);

        SkipWS(P); Expect(P,'{');
        LoopLbl L{}; L.condOff=condOff; L.contTarget=condOff;
        loopStack.push_back(L);

        size_t depth=1;
        while(!AtEnd(P)){
          if((*P.src)[P.i]=='{'){ ++depth; ++P.i; continue; }
          if((*P.src)[P.i]=='}'){ --depth; ++P.i; if(depth==0) break; continue; }
          StmtSeqOne();
        }

        emit_u8(OP_JMP); emit_u64(condOff);
        auto after = tell();
        patchJump(jfAt, after);
        for(auto site : loopStack.back().breakPatchSites) patchJump(site, after);
        loopStack.pop_back();
        continue;
      }

      // if (...) { ... } [elif (...) { ... }]* [else { ... }]
      if(StartsWithKW(P,"if")){
        P.i+=2; SkipWS(P); Expect(P,'('); Expr(); Expect(P,')');
        emit_u8(OP_JF); auto jfAt = tell(); emit_u64(0);
        SkipWS(P); Expect(P,'{'); StmtSeqUntilBrace();

        emit_u8(OP_JMP); auto jendAt=tell(); emit_u64(0);
        auto afterThen = tell(); patchJump(jfAt, afterThen);

        std::vector<uint64_t> ends; ends.push_back(jendAt);
        while(true){
          Pos peek=P; SkipWS(peek); if(!StartsWithKW(peek,"elif")) break;
          P.i=peek.i+4; SkipWS(P); Expect(P,'('); Expr(); Expect(P,')');
          emit_u8(OP_JF); auto ejf=tell(); emit_u64(0);
          SkipWS(P); Expect(P,'{'); StmtSeqUntilBrace();
          emit_u8(OP_JMP); auto ejend=tell(); emit_u64(0);
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

      // let [type|auto] name = expr ;
      if(StartsWithKW(P,"let")){
        P.i+=3; SkipWS(P);
        bool isAuto=false; Type T=Type::Int;
        if(StartsWithKW(P,"auto")){ P.i+=4; isAuto=true; }
        else { T=parseType(); }
        SkipWS(P); if(Match(P,':')){}
        auto name=ParseIdent(P); SkipWS(P); Expect(P,'=');
        Expr(); Expect(P,';');
        emit_u8(OP_DECL); emit_str(name); emit_u8(isAuto?0xFF:(uint8_t)T);
        continue;
      }

      // assignment or call;
      if(!AtEnd(P)&&IsIdStart((*P.src)[P.i])){
        size_t save=P.i; auto name=ParseIdent(P); SkipWS(P);
        if(!AtEnd(P) && (*P.src)[P.i]=='='){
          ++P.i; Expr(); Expect(P,';'); emit_u8(OP_SET); emit_str(name); continue;
        } else {
          P.i=save; Expr(); Expect(P,';'); emit_u8(OP_POP); continue;
        }
      }

      throw ScriptError("unexpected token", Span{P.i,P.i+1});
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
    table_offset_pos = (uint64_t)ftell(out); write_u64(out, 0); // table_offset
    fn_count_pos     = (uint64_t)ftell(out); write_u64(out, 0); // count
    entry_main_pos   = (uint64_t)ftell(out); write_u64(out, 0); // entry_main
  }

  inline void compileToFile(const std::string& outPath){
    out = fopen(outPath.c_str(),"wb+"); if(!out) throw std::runtime_error("cannot open bytecode file for write");
    writeHeaderPlaceholders();

    // Top-level as __main__
    FnInfo mainFn{"__main__",0,{},true,false,Type::Int};
    fns.push_back(mainFn); fnIndex["__main__"]=0;
    fns[0].entry = tell();

    P.i=0;
    SkipWS(P);
    StmtSeq();
    emit_u8(OP_HALT);

    // Write function table at EOF; remember offset
    uint64_t tbl_off = tell();
    uint64_t count = (uint64_t)fns.size();
    for(auto&fn: fns){
      write_str(out, fn.name); write_u64(out, fn.entry);
      write_u8(out, fn.isVoid?1:0); write_u8(out, fn.typed?1:0); write_u8(out, (uint8_t)fn.ret);
      write_u64(out, (uint64_t)fn.params.size());
      for(auto&s: fn.params) write_str(out, s);
    }
    fflush(out);
    // patch header
    seek(table_offset_pos); write_u64(out, tbl_off);
    seek(fn_count_pos);     write_u64(out, count);
    seek(entry_main_pos);   write_u64(out, fns[0].entry);
    fclose(out); out=nullptr;
  }
};

/*=============================
=              VM            =
=============================*/
struct VM {
  Env globals;

  FILE* f=nullptr; uint64_t ip=0, table_off=0, code_end=0;
  std::vector<Value> stack;

// in struct VM { ... }
  struct Frame{
    uint64_t ret_ip;
    std::unique_ptr<Env> env;   // heap-allocated, stable address
    bool isVoid = false;
    bool typed  = false;
    Type ret    = Type::Int;
  }; // <-- semicolon!

  std::vector<Frame> frames;

  Builtins* builtins=nullptr;
  struct FnMeta{ uint64_t entry; bool isVoid; bool typed; Type ret; std::vector<std::string> params; };
  std::unordered_map<std::string, FnMeta> fnEntry;

  explicit VM(Builtins*b):builtins(b){}

  inline void jump(uint64_t target){ ip = target; fseek(f,(long)ip,SEEK_SET); }
  inline uint8_t  fetch8(){ uint8_t v; fread(&v,1,1,f); ++ip; return v; }
  inline uint64_t fetch64(){ uint64_t v; fread(&v,8,1,f); ip+=8; return v; }
  inline int64_t  fetchs64(){ int64_t v; fread(&v,8,1,f); ip+=8; return v; }
  inline double   fetchf64(){ double v; fread(&v,8,1,f); ip+=8; return v; }
  inline std::string fetchStr(){ uint64_t n=fetch64(); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); ip+=n; return s; }

  inline Value pop(){ if(stack.empty()) throw ScriptError("stack underflow"); Value v=std::move(stack.back()); stack.pop_back(); return v; }
  inline void push(Value v){ stack.push_back(std::move(v)); }

  inline Value coerceTo(Type t, Value v){
    if (t==v.t) return v;
    if (t==Type::Int)   return Value::I(v.AsInt());
    if (t==Type::Float) return Value::F(v.AsFloat());
    if (t==Type::Bool)  return Value::B(v.AsBool());
    if (t==Type::Str)   { if(v.t!=Type::Str)  throw ScriptError("return type mismatch: need str");  return v; }
    if (t==Type::List)  { if(v.t!=Type::List) throw ScriptError("return type mismatch: need list"); return v; }
    return v;
  }

  inline void load(const std::string& path){
    f=fopen(path.c_str(),"rb"); if(!f) throw std::runtime_error("cannot open bytecode");
    char magic[9]={0}; fread(magic,1,8,f); if(std::string(magic)!="AVOCADO1") throw std::runtime_error("bad bytecode magic");
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

  inline bool ReadSGRMouse(std::string& kind, int& button, int& x, int& y) {
    auto empty = [](){ return false; };
    if (!std::cin.good() || std::cin.rdbuf()->in_avail() <= 0) return false;

    int c1 = std::cin.peek();
    if (c1 != 0x1B) return false;
    std::cin.get(); // ESC

    if (std::cin.rdbuf()->in_avail() <= 0) { std::cin.putback((char)0x1B); return empty(); }
    if (std::cin.peek() != '[')            { std::cin.putback((char)0x1B); return empty(); }
    std::cin.get(); // '['

    if (std::cin.rdbuf()->in_avail() <= 0) { std::cin.putback('['); std::cin.putback((char)0x1B); return empty(); }
    if (std::cin.peek() != '<')            { std::cin.putback('['); std::cin.putback((char)0x1B); return empty(); }
    std::cin.get(); // '<'

    auto readInt = [] (int& out)->bool {
      std::string num;
      while (std::cin.good() && std::cin.rdbuf()->in_avail() > 0) {
        int p = std::cin.peek(); if (p < 0 || !std::isdigit((unsigned char)p)) break;
        num.push_back((char)std::cin.get());
      }
      if (num.empty()) return false;
      try { out = std::stoi(num); } catch (...) { return false; }
      return true;
    };

    int b=0; x=0; y=0;
    if (!readInt(b)) return empty();
    if (std::cin.rdbuf()->in_avail()<=0 || std::cin.peek()!=';') return empty();
    std::cin.get();
    if (!readInt(x)) return empty();
    if (std::cin.rdbuf()->in_avail()<=0 || std::cin.peek()!=';') return empty();
    std::cin.get();
    if (!readInt(y)) return empty();
    if (std::cin.rdbuf()->in_avail()<=0)  return empty();

    int term = std::cin.get(); // 'M' (press/drag) or 'm' (release)
    if (term!='M' && term!='m') return empty();

    // classify
    if (b & 64)      kind = (b & 1) ? "wheel-down" : "wheel-up";
    else if (b & 32) kind = "drag";
    else             kind = (term=='M') ? "down" : "up";
    button = (b & 3) + 1;
    return true;
  }


inline void run(){
  for(;;){
    if (ip >= code_end) return;
    uint8_t op = fetch8();

    switch (op) {
      case OP_HALT: return;
      case OP_NOP:  break;

      case OP_PUSH_I: { push(Value::I(fetchs64())); } break;
      case OP_PUSH_F: { push(Value::F(fetchf64())); } break;
      case OP_PUSH_B: { push(Value::B(fetch8()!=0)); } break;
      case OP_PUSH_S: { auto s = fetchStr(); push(Value::S(std::move(s))); } break;

      case OP_MAKE_LIST: {
        uint64_t n = fetch64();
        std::vector<Value> xs; xs.resize(n);
        for (uint64_t i=0; i<n; ++i) xs[n-1-i] = pop();
        push(Value::L(std::move(xs)));
      } break;

      case OP_GET: {
        auto id = fetchStr();
        push(frames.back().env->Get(id).val);
      } break;

      case OP_SET: {
        auto id = fetchStr();
        auto v  = pop();
        frames.back().env->SetOrDeclare(id, v);
      } break;

      case OP_DECL: {
        auto id = fetchStr();
        uint8_t tt = fetch8();
        auto v  = pop();
        if (tt == 0xFF) frames.back().env->Declare(id, v.t, v);
        else            frames.back().env->Declare(id, (Type)tt, v);
      } break;

      case OP_POP: { (void)pop(); } break;

      case OP_ADD: {
        auto b = pop();
        auto a = pop();

        if (a.t == Type::List) {
          Value out = a; // copy the left list
          auto& L = std::get<std::vector<Value>>(out.v);
          if (b.t == Type::List) {
            const auto& R = std::get<std::vector<Value>>(b.v);
            L.insert(L.end(), R.begin(), R.end());   // concat
          } else {
            L.push_back(b); // push
          }
            push(out);
        } else if (a.t == Type::Str || b.t == Type::Str) {
          push(Value::S(a.AsStr() + b.AsStr()));
        } else if (a.t == Type::Float || b.t == Type::Float) {
          push(Value::F(a.AsFloat() + b.AsFloat()));
        } else {
          push(Value::I(a.AsInt() + b.AsInt()));
        }
      } break;

      case OP_UNSET: {
      auto id = fetchStr();
        #if 1
          // Use Env::Unset if you added it (see step 2)
          if (!frames.back().env->Unset(id))
            throw ScriptError(std::string("unknown variable: ") + id);
        #else
          // Inline scope-walk if you skipped step 2
          Env* cur = frames.back().env.get();
          bool ok = false;
          while (cur && !ok){ ok = cur->Erase(id); cur = cur->parent; }
          if (!ok) throw ScriptError(std::string("unknown variable: ") + id);
        #endif
      } break;

      case OP_SUB: {
        auto b = pop(), a = pop();
        if (a.t==Type::Float || b.t==Type::Float) push(Value::F(a.AsFloat()-b.AsFloat()));
        else push(Value::I(a.AsInt()-b.AsInt()));
      } break;

      case OP_MUL: {
        auto b = pop(), a = pop();
        if (a.t==Type::Float || b.t==Type::Float) push(Value::F(a.AsFloat()*b.AsFloat()));
        else push(Value::I(a.AsInt()*b.AsInt()));
      } break;

      case OP_DIV: {
        auto b = pop(), a = pop();
        push(Value::F(a.AsFloat()/b.AsFloat()));
      } break;

      case OP_EQ: {
        auto b = pop(), a = pop();
        bool eq = (a.t==b.t) ? (a==b)
                  : ((a.t!=Type::Str && a.t!=Type::List && b.t!=Type::Str && b.t!=Type::List)
                      ? (a.AsFloat()==b.AsFloat()) : false);
        push(Value::B(eq));
      } break;

      case OP_NE: {
        auto b = pop(), a = pop();
        bool ne = (a.t==b.t) ? !(a==b)
                  : ((a.t!=Type::Str && a.t!=Type::List && b.t!=Type::Str && b.t!=Type::List)
                      ? (a.AsFloat()!=b.AsFloat()) : true);
        push(Value::B(ne));
      } break;

      case OP_LT: {
        auto b = pop(), a = pop();
        if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()<b.AsStr()));
        else push(Value::B(a.AsFloat()<b.AsFloat()));
      } break;

      case OP_LE: {
        auto b = pop(), a = pop();
        if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()<=b.AsStr()));
        else push(Value::B(a.AsFloat()<=b.AsFloat()));
      } break;

      case OP_GT: {
        auto b = pop(), a = pop();
        if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()>b.AsStr()));
        else push(Value::B(a.AsFloat()>b.AsFloat()));
      } break;

      case OP_GE: {
        auto b = pop(), a = pop();
        if (a.t==Type::Str && b.t==Type::Str) push(Value::B(a.AsStr()>=b.AsStr()));
        else push(Value::B(a.AsFloat()>=b.AsFloat()));
      } break;

      case OP_AND: { auto b = pop(), a = pop(); push(Value::B(a.AsBool() && b.AsBool())); } break;
      case OP_OR : { auto b = pop(), a = pop(); push(Value::B(a.AsBool() || b.AsBool())); } break;

      case OP_JMP: { auto tgt = fetch64(); jump(tgt); } break;
      case OP_JF : { auto tgt = fetch64(); auto v = pop(); if (!v.AsBool()) jump(tgt); } break;

      case OP_CALL_BUILTIN: {
        auto name = fetchStr();
        uint64_t argc = fetch64();
        std::vector<Value> args(argc);
        for (size_t i=0;i<argc;++i) args[argc-1-i] = pop();

        auto it = builtins->f.find(name);
        if (it == builtins->f.end()) throw ScriptError("unknown builtin: " + name);

        auto out = it->second(args);
        if (out.has_value()) push(*out);
        else push(Value::I(0)); // VOID builtin: push dummy to keep POP safe
      } break;

      case OP_CALL_USER: {
        auto name = fetchStr();
        uint64_t argc = fetch64();
        std::vector<Value> args(argc);
        for (size_t i=0;i<argc;++i) args[argc-1-i] = pop();

        auto it = fnEntry.find(name);
        if (it == fnEntry.end()) throw ScriptError("unknown function: " + name);
        const auto& meta = it->second;

        // new frame; parent = caller's env; heap env is stable
        frames.push_back(Frame{ ip, nullptr, meta.isVoid, meta.typed, meta.ret });
        Env* callerEnv = frames[frames.size()-2].env.get();
        frames.back().env = std::make_unique<Env>(callerEnv);

        // bind parameters
        for (size_t i=0; i<meta.params.size() && i<args.size(); ++i) {
          frames.back().env->Declare(meta.params[i], args[i].t, args[i]);
        }

        jump(meta.entry);
      } break;

      case OP_RET: {
        Value rv = pop();
        if (frames.size()==1) return;
        if (frames.back().typed) rv = coerceTo(frames.back().ret, rv);
        uint64_t ret = frames.back().ret_ip;
        frames.pop_back();
        jump(ret);
        push(rv);
      } break;

      case OP_MOUSE: {
      std::string kind; int btn=0, x=1, y=1;
      if (!ReadSGRMouse(kind, btn, x, y)) {
        // push []
        std::vector<Value> v;
        push(Value::L(std::move(v)));
      } else {
        std::vector<Value> v;
        v.emplace_back(Value::S(kind));
        v.emplace_back(Value::I(btn));
        v.emplace_back(Value::I(x));
        v.emplace_back(Value::I(y));
        push(Value::L(std::move(v)));
      }
    } break;


      case OP_RET_VOID: {
        if (frames.size()==1) return;
        uint64_t ret = frames.back().ret_ip;
        frames.pop_back();
        jump(ret);
        push(Value::I(0));  // VOID fn: dummy for trailing POP
      } break;

      default:
        throw ScriptError("Bad opcode. If you wrote the MSBC file yourself, check for errors.");
    } // switch
  } // for
} // run()
};

/*=============================
=        Public API           =
=============================*/
inline std::string read_file(const std::string& path){
  std::ifstream in(path, std::ios::binary);
  if(!in) throw std::runtime_error("cannot open: "+path);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
inline void compile_file_to_avocado(const std::string& srcName, const std::string& srcText, const std::string& outAvo){
  Builtins b; Source S{srcName, srcText}; Compiler C(S,&b); C.compileToFile(outAvo);
}
inline void run_avocado(const std::string& bcPath){
  Builtins b; VM vm(&b); vm.load(bcPath); vm.run();
}
} // namespace minis
#endif // MINIS_HPP_INCLUDED
