// minis.cpp — single-file engine with lexical scoping, funcs, elif, &&, ||,
// Load/Save, raw TTY-safe print, mouse/keyboard, and diagnostics-friendly TTY restore.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <cctype>
#include <variant>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <optional>
#include <random>
#include <fstream>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

#include "diagnostics.h"  // defines ::Source (name, text, line_starts, etc.)
// Bytecode codes
// Use for string lookup instead of function and action lookup
// Minis is no longer interpreted, but compiled into bytecode


// -----------------------------------------------
// Minis
// -----------------------------------------------
namespace minis {

// ---------- scanning helpers ----------
struct Pos {
    size_t i = 0;
    const std::string* src = nullptr;
};

static inline bool AtEnd(Pos& p){ return p.i >= p.src->size(); }
static inline bool IsIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_'; }
static inline bool IsIdentCont (char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='.'; }

static inline void SkipWS(Pos& p){
    const std::string& s = *p.src;
    while (true){
        while (!AtEnd(p) && std::isspace((unsigned char)s[p.i])) ++p.i;
        if (!AtEnd(p) && p.i+1<s.size() && s[p.i]=='/' && s[p.i+1]=='/'){
            p.i += 2; while (!AtEnd(p) && s[p.i] != '\n') ++p.i; continue;
        }
        if (!AtEnd(p) && p.i+1<s.size() && s[p.i]=='/' && s[p.i+1]=='*'){
            p.i += 2; while (p.i+1<s.size() && !(s[p.i]=='*' && s[p.i+1]=='/')) ++p.i;
            if (p.i+1<s.size()) p.i += 2; continue;
        }
        break;
    }
}
static inline bool Match(Pos& p, char c){
    SkipWS(p);
    if (!AtEnd(p) && (*p.src)[p.i]==c){ ++p.i; return true; }
    return false;
}
static inline bool MatchStr(Pos& p, const char* s){
    SkipWS(p);
    size_t L = std::strlen(s);
    if (p.i + L <= p.src->size() && p.src->compare(p.i, L, s) == 0){ p.i += L; return true; }
    return false;
}
static inline void Expect(Pos& p, char c){
    SkipWS(p);
    size_t where = p.i;
    if (AtEnd(p) || (*p.src)[p.i] != c)
        throw ScriptError(std::string("expected '") + c + "'", Span{where, where+1});
    ++p.i;
}
static inline std::string ParseIdent(Pos& p){
    SkipWS(p);
    size_t s = p.i;
    if (AtEnd(p) || !IsIdentStart((*p.src)[p.i])) throw ScriptError("expected identifier", Span{s,s+1});
    ++p.i; while (!AtEnd(p) && IsIdentCont((*p.src)[p.i])) ++p.i;
    return p.src->substr(s, p.i - s);
}
static inline std::string ParseQuoted(Pos& p){
    SkipWS(p);
    if (AtEnd(p)) throw ScriptError("expected string", Span{p.i,p.i});
    char q = (*p.src)[p.i];
    if (q!='"' && q!='\'') throw ScriptError("expected string", Span{p.i,p.i+1});
    ++p.i;
    std::string out;
    while (!AtEnd(p) && (*p.src)[p.i] != q){
        char c = (*p.src)[p.i++];
        if (c=='\\'){
            if (AtEnd(p)) throw ScriptError("unterminated string", Span{p.i,p.i});
            char n = (*p.src)[p.i++];
            switch(n){
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '\'': out.push_back('\''); break;
                default: out.push_back(n); break;
            }
        } else out.push_back(c);
    }
    if (AtEnd(p) || (*p.src)[p.i]!=q) throw ScriptError("unterminated string", Span{p.i,p.i});
    ++p.i;
    return out;
}
static inline std::string ParseNumberText(Pos& p){
    SkipWS(p);
    size_t s = p.i;
    if (!AtEnd(p) && (((*p.src)[p.i]=='+') || ((*p.src)[p.i]=='-'))) ++p.i;
    bool seenDigits=false, seenDot=false;
    while (!AtEnd(p)){
        char c = (*p.src)[p.i];
        if (std::isdigit((unsigned char)c)){ seenDigits=true; ++p.i; }
        else if (c=='.' && !seenDot){ seenDot=true; ++p.i; }
        else break;
    }
    if (!seenDigits) throw ScriptError("expected number", Span{s,s+1});
    return p.src->substr(s, p.i - s);
}
static inline bool StartsWithKW(Pos& p, const char* kw){
    SkipWS(p);
    size_t s = p.i, L = std::strlen(kw);
    if (s+L > p.src->size()) return false;
    if (p.src->compare(s, L, kw) != 0) return false;
    auto iscont = [](char c){ return std::isalnum((unsigned char)c) || c=='_'; };
    bool leftOk  = (s==0) || !iscont((*p.src)[s-1]);
    bool rightOk = (s+L>=p.src->size()) || !iscont((*p.src)[s+L]);
    return leftOk && rightOk;
}

// ---------- values ----------
enum class Type { Int, Float, Bool, Str, List };

struct Value;
using ValueList = std::vector<Value>;

struct Value {
    Type t;
    std::variant<long long,double,bool,std::string,std::vector<Value>> v;

    static Value MakeInt  (long long x){ return {Type::Int,   x}; }
    static Value MakeFloat(double    x){ return {Type::Float, x}; }
    static Value MakeBool (bool      x){ return {Type::Bool,  x}; }
    static Value MakeStr  (std::string s){ return {Type::Str,  std::move(s)}; }
    static Value MakeList (std::vector<Value> xs){ return {Type::List, std::move(xs)}; }

    long long AsInt() const {
        switch(t){
            case Type::Int:   return std::get<long long>(v);
            case Type::Float: return (long long)std::get<double>(v);
            case Type::Bool:  return std::get<bool>(v) ? 1 : 0;
            default: throw ScriptError("cannot convert to int", Span{});
        }
    }
    double AsFloat() const {
        switch(t){
            case Type::Int:   return (double)std::get<long long>(v);
            case Type::Float: return std::get<double>(v);
            case Type::Bool:  return std::get<bool>(v) ? 1.0 : 0.0;
            default: throw ScriptError("cannot convert to float", Span{});
        }
    }
    bool AsBool() const {
        switch(t){
            case Type::Bool:  return std::get<bool>(v);
            case Type::Int:   return std::get<long long>(v)!=0;
            case Type::Float: return std::get<double>(v)!=0.0;
            case Type::Str:   return !std::get<std::string>(v).empty();
            case Type::List:  return !std::get<std::vector<Value>>(v).empty();
        }
        return false;
    }
    std::string AsStr() const {
        switch(t){
            case Type::Str:   return std::get<std::string>(v);
            case Type::Int:   return std::to_string(std::get<long long>(v));
            case Type::Float: { std::ostringstream os; os<<std::get<double>(v); return os.str(); }
            case Type::Bool:  return std::get<bool>(v) ? "true" : "false";
            case Type::List: {
                const auto& xs = std::get<std::vector<Value>>(v);
                std::ostringstream os; os << "[";
                for (size_t i=0;i<xs.size();++i){ if(i) os<<","; os<<xs[i].AsStr(); }
                os << "]";
                return os.str();
            }
        }
        return {};
    }
    const std::vector<Value>& AsList() const { if(t!=Type::List) throw ScriptError("expected list", Span{}); return std::get<std::vector<Value>>(v); }
    std::vector<Value>&       AsList()       { if(t!=Type::List) throw ScriptError("expected list", Span{}); return std::get<std::vector<Value>>(v); }
};

inline bool operator==(const Value& a, const Value& b){
    if (a.t != b.t) return false;
    switch (a.t){
        case Type::Int:   return std::get<long long>(a.v) == std::get<long long>(b.v);
        case Type::Float: return std::get<double>(a.v)    == std::get<double>(b.v);
        case Type::Bool:  return std::get<bool>(a.v)      == std::get<bool>(b.v);
        case Type::Str:   return std::get<std::string>(a.v) == std::get<std::string>(b.v);
        case Type::List:  return std::get<std::vector<Value>>(a.v) == std::get<std::vector<Value>>(b.v);
    }
    return false;
}
inline bool operator!=(const Value& a, const Value& b){ return !(a==b); }
inline bool operator>(const Value& a, const Value& b){
    auto num = [](Type t){ return t==Type::Int||t==Type::Float||t==Type::Bool; };
    if (num(a.t) && num(b.t)) return a.AsFloat() > b.AsFloat();
    if (a.t==Type::Str && b.t==Type::Str) return std::get<std::string>(a.v) > std::get<std::string>(b.v);
    return false;
}
/*
inline bool operator>=(const Value& a, const Value& b){
    auto num = [](Type t){ return t==Type::Int||t==Type::Float||t==Type::Bool; };
    if (num(a.t) && num(b.t)) return a.AsFloat() >= b.AsFloat();
    if (a.t==Type::Str && b.t==Type::Str) return std::get<std::string>(a.v) >= std::get<std::string>(b.v);
    return false;
}
inline bool operator<(const Value& a, const Value& b){
    auto num = [](Type t){ return t==Type::Int||t==Type::Float||t==Type::Bool; };
    if (num(a.t) && num(b.t)) return a.AsFloat() < b.AsFloat();
    if (a.t==Type::Str && b.t==Type::Str) return std::get<std::string>(a.v) > std::get<std::string>(b.v);
    return false;
}
inline bool operator<=(const Value& a, const Value& b){
    auto num = [](Type t){ return t==Type::Int||t==Type::Float||t==Type::Bool; };
    if (num(a.t) && num(b.t)) return a.AsFloat() <= b.AsFloat();
    if (a.t==Type::Str && b.t==Type::Str) return std::get<std::string>(a.v) >= std::get<std::string>(b.v);
    return false;
}
*/
// ---------- environment (lexical) ----------
struct Var { Type declared; Value val; };

class Env {
    std::unordered_map<std::string, Var> m;

    static void AssignChecked(Type t, Value& v){
        if (t==v.t) return;
        switch(t){
            case Type::Int:   v = Value::MakeInt  (v.AsInt());   break;
            case Type::Float: v = Value::MakeFloat(v.AsFloat()); break;
            case Type::Bool:  v = Value::MakeBool (v.AsBool());  break;
            case Type::Str:   if (v.t!=Type::Str)  throw ScriptError("cannot assign non-str to str",   Span{}); break;
            case Type::List:  if (v.t!=Type::List) throw ScriptError("cannot assign non-list to list", Span{}); break;
        }
    }
public:
    Env* parent = nullptr;            // lexical parent (nullptr = top)

    Env(Env* p=nullptr): parent(p) {}

    bool ExistsLocal(const std::string& n) const { return m.find(n) != m.end(); }
    bool Exists      (const std::string& n) const { return ExistsLocal(n) || (parent && parent->Exists(n)); }

    const Var& GetLocal(const std::string& n) const {
        auto it = m.find(n); if (it==m.end()) throw ScriptError("unknown variable: " + n, Span{}); return it->second;
    }
    const Var& Get(const std::string& n) const {
        auto it = m.find(n);
        if (it != m.end()) return it->second;
        if (parent) return parent->Get(n);
        throw ScriptError("unknown variable: " + n, Span{});
    }

    void Declare(const std::string& n, Type t, Value v){
        if (m.count(n)) throw ScriptError("variable already declared: " + n, Span{});
        AssignChecked(t, v); m.emplace(n, Var{t, v});
    }
    void SetLocal(const std::string& n, Value v){
        auto it = m.find(n); if (it==m.end()) throw ScriptError("unknown variable: " + n, Span{});
        AssignChecked(it->second.declared, v); it->second.val = v;
    }
    void Set(const std::string& n, Value v){
        auto it = m.find(n);
        if (it != m.end()){ AssignChecked(it->second.declared, v); it->second.val = v; return; }
        if (parent){ parent->Set(n, v); return; }
        throw ScriptError("unknown variable: " + n, Span{});
    }
    void SetOrDeclare(const std::string& n, Value v){
        if (ExistsLocal(n)) { SetLocal(n, v); return; }
        if (parent && parent->Exists(n)) { parent->Set(n, v); return; }
        Declare(n, v.t, v);
    }
    bool Erase(const std::string& n){ return m.erase(n) != 0; }
};

// ---------- parser (expressions only) ----------
class Parser {
public:
    Pos& p; Env* env; const ::Source* src;
    std::unordered_map<std::string, std::function<std::optional<Value>(const std::vector<Value>&)>>* fnmap;

    Parser(Pos& P, Env* E, const ::Source* S,
           std::unordered_map<std::string, std::function<std::optional<Value>(const std::vector<Value>&)>>* F)
        : p(P), env(E), src(S), fnmap(F) {}

    Value ParseExpr(){ return ParseLogicOr(); }

private:
    Value ParseLogicOr(){
        Value v = ParseLogicAnd();
        while (MatchStr(p,"||")){
            Value r = ParseLogicAnd();
            v = Value::MakeBool(v.AsBool() || r.AsBool());
        }
        return v;
    }
    Value ParseLogicAnd(){
        Value v = ParseEquality();
        while (MatchStr(p,"&&")){
            Value r = ParseEquality();
            v = Value::MakeBool(v.AsBool() && r.AsBool());
        }
        return v;
    }
    Value ParseEquality(){
        Value v = ParseAddSub();
        while (true){
            if (MatchStr(p,"==")){
                Value r = ParseAddSub();
                bool eq=false;
                if (v.t == r.t){
                    switch(v.t){
                        case Type::Int:   eq = (std::get<long long>(v.v) == std::get<long long>(r.v)); break;
                        case Type::Float: eq = (std::get<double>(v.v)    == std::get<double>(r.v));    break;
                        case Type::Bool:  eq = (std::get<bool>(v.v)      == std::get<bool>(r.v));      break;
                        case Type::Str:   eq = (std::get<std::string>(v.v) == std::get<std::string>(r.v)); break;
                        case Type::List:  eq = (std::get<std::vector<Value>>(v.v) == std::get<std::vector<Value>>(r.v)); break;
                    }
                } else {
                    bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                    bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                    eq = (isNumL && isNumR) ? (v.AsFloat()==r.AsFloat()) : false;
                }
                v = Value::MakeBool(eq);
            } else if (MatchStr(p,">=")){
                Value r = ParseAddSub();
                bool ge=false;
                bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                if (isNumL && isNumR) ge = (v.AsFloat() >= r.AsFloat());
                else if (v.t==Type::Str && r.t==Type::Str) ge = (std::get<std::string>(v.v) >= std::get<std::string>(r.v));
                v = Value::MakeBool(ge);
            } else if (MatchStr(p,">")){
                Value r = ParseAddSub();
                bool gt=false;
                bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                if (isNumL && isNumR) gt = (v.AsFloat() > r.AsFloat());
                else if (v.t==Type::Str && r.t==Type::Str) gt = (std::get<std::string>(v.v) > std::get<std::string>(r.v));
                v = Value::MakeBool(gt);
            } else if (MatchStr(p,"<=")){
                Value r = ParseAddSub();
                bool le = false;
                bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                if (isNumL && isNumR) le = (v.AsFloat() <= r.AsFloat());
                else if (v.t==Type::Str && r.t==Type::Str) le = (std::get<std::string>(v.v) <= std::get<std::string>(r.v));
                v = Value::MakeBool(le);
            } else if (MatchStr(p,"<")){
                Value r = ParseAddSub();
                bool lt = false;
                bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                if (isNumL && isNumR) lt = (v.AsFloat() < r.AsFloat());
                else if (v.t==Type::Str && r.t==Type::Str) lt = (std::get<std::string>(v.v) < std::get<std::string>(r.v));
                v = Value::MakeBool(lt);
            } else if (MatchStr(p,"!=")){
                Value r = ParseAddSub();
                bool ne=true;
                if (v.t == r.t){
                    switch(v.t){
                        case Type::Int:   ne = (std::get<long long>(v.v) != std::get<long long>(r.v)); break;
                        case Type::Float: ne = (std::get<double>(v.v)    != std::get<double>(r.v));    break;
                        case Type::Bool:  ne = (std::get<bool>(v.v)      != std::get<bool>(r.v));      break;
                        case Type::Str:   ne = (std::get<std::string>(v.v) != std::get<std::string>(r.v)); break;
                        case Type::List:  ne = (std::get<std::vector<Value>>(v.v) != std::get<std::vector<Value>>(r.v)); break;
                    }
                } else {
                    bool isNumL = (v.t==Type::Int || v.t==Type::Float || v.t==Type::Bool);
                    bool isNumR = (r.t==Type::Int || r.t==Type::Float || r.t==Type::Bool);
                    ne = (isNumL && isNumR) ? (v.AsFloat()!=r.AsFloat()) : true;
                }
                v = Value::MakeBool(ne);
            } else break;
        }
        return v;
    }
    Value ParseAddSub(){
        Value v = ParseMulDiv();
        while (true){
            SkipWS(p);
            if (Match(p,'+')){
                Value r = ParseMulDiv();
                if (v.t==Type::Str || r.t==Type::Str) v = Value::MakeStr(v.AsStr()+r.AsStr());
                else if (v.t==Type::Float || r.t==Type::Float) v = Value::MakeFloat(v.AsFloat()+r.AsFloat());
                else v = Value::MakeInt(v.AsInt()+r.AsInt());
            } else if (Match(p,'-')){
                Value r = ParseMulDiv();
                if (v.t==Type::Str || r.t==Type::Str || v.t==Type::List || r.t==Type::List)
                    throw ScriptError("cannot subtract these types", Span{});
                if (v.t==Type::Float || r.t==Type::Float) v = Value::MakeFloat(v.AsFloat()-r.AsFloat());
                else v = Value::MakeInt(v.AsInt()-r.AsInt());
            } else break;
        }
        return v;
    }
    Value ParseMulDiv(){
        Value v = ParseFactor();
        while (true){
            SkipWS(p);
            if (Match(p,'*')){
                Value r = ParseFactor();
                if (v.t==Type::Str || r.t==Type::Str || v.t==Type::List || r.t==Type::List)
                    throw ScriptError("cannot multiply these types", Span{});
                if (v.t==Type::Float || r.t==Type::Float) v = Value::MakeFloat(v.AsFloat()*r.AsFloat());
                else v = Value::MakeInt(v.AsInt()*r.AsInt());
            } else if (Match(p,'/')){
                Value r = ParseFactor();
                if (v.t==Type::Str || r.t==Type::Str || v.t==Type::List || r.t==Type::List)
                    throw ScriptError("cannot divide these types", Span{});
                v = Value::MakeFloat(v.AsFloat()/r.AsFloat());
            } else break;
        }
        return v;
    }
    Value ParseListLit(){
        std::vector<Value> xs;
        SkipWS(p);
        if (Match(p,']')) return Value::MakeList(std::move(xs));
        while (true){
            Value item = ParseExpr();
            xs.push_back(item);
            SkipWS(p);
            if (Match(p,']')) break;
            Expect(p,',');
        }
        return Value::MakeList(std::move(xs));
    }
    Value ParseFactor(){
        SkipWS(p);
        if (!AtEnd(p) && (*p.src)[p.i]=='('){ ++p.i; Value v=ParseExpr(); Expect(p,')'); return v; }
        if (!AtEnd(p) && ((*p.src)[p.i]=='"' || (*p.src)[p.i]=='\'')) return Value::MakeStr(ParseQuoted(p));
        if (p.i+4<=p.src->size() && p.src->compare(p.i,4,"true")==0  && (p.i+4==p.src->size() || !IsIdentCont((*p.src)[p.i+4]))){ p.i+=4; return Value::MakeBool(true); }
        if (p.i+5<=p.src->size() && p.src->compare(p.i,5,"false")==0 && (p.i+5==p.src->size() || !IsIdentCont((*p.src)[p.i+5]))){ p.i+=5; return Value::MakeBool(false); }
        if (!AtEnd(p) && (*p.src)[p.i]=='['){ ++p.i; return ParseListLit(); }
        if (!AtEnd(p) && (std::isdigit((unsigned char)(*p.src)[p.i]) || (*p.src)[p.i]=='+' || (*p.src)[p.i]=='-')){
            auto s = ParseNumberText(p);
            if (s.find('.')!=std::string::npos) return Value::MakeFloat(std::stod(s));
            return Value::MakeInt(std::stoll(s));
        }
        if (!AtEnd(p) && IsIdentStart((*p.src)[p.i])){
            std::string id = ParseIdent(p);
            SkipWS(p);
            if (!AtEnd(p) && (*p.src)[p.i]=='('){
                ++p.i;
                std::vector<Value> args;
                SkipWS(p);
                if (!Match(p,')')){
                    while (true){
                        Parser ex(p, env, src, fnmap);
                        Value v = ex.ParseExpr();
                        args.push_back(v);
                        SkipWS(p);
                        if (Match(p,')')) break;
                        Expect(p, ',');
                    }
                }
                if (!fnmap) throw ScriptError("function calls not allowed here", Span{});
                auto it = fnmap->find(id);
                if (it==fnmap->end()) throw ScriptError("unknown function: " + id, Span{});
                std::optional<Value> out;
                try { out = it->second(args); }
                catch (ScriptError& se){ se.notes.push_back("in call to '" + id + "'"); throw; }
                catch (std::exception& se){
                    ScriptError e(std::string("runtime: ") + se.what(), Span{});
                    e.notes.push_back("in call to '" + id + "'"); throw e;
                }
                if (!out.has_value()) throw ScriptError("function returns no value", Span{});
                return *out;
            } else {
                return env->Get(id).val;
            }
        }
        throw ScriptError("unexpected token", Span{p.i,p.i+1});
    }
};

// ---------- engine ----------
struct Frame { std::string fn; Span call; };

class Engine {
public:
    using Handler = std::function<std::optional<Value>(const std::vector<Value>&)>;

    // public so host can set source and print stack
    const ::Source* src = nullptr;
    std::vector<Frame>  stack;

    // environments
    Env  env;          // top-level
    Env* cur = &env;   // current lexical scope (always use this)

    // user functions
    struct UFunc {
    std::string name;
    std::vector<std::string> params;
    size_t bodyBeg = 0;  // first char after '{'
    size_t bodyEnd = 0;  // index of matching '}'

    bool isVoid = false;
    Type retType = Type::Int;  // ignored when isVoid==true

    bool hasExplicitRet = false;
};


    std::unordered_map<std::string, UFunc> ufns;

    // host I/O & builtins
    std::unordered_map<std::string, Handler> fns;
    bool silentIO = false;

#ifdef _WIN32
    DWORD oldIn = 0, oldOut = 0;
#else
    termios oldTio{};
    bool haveOld = false;
#endif

    // ---- public helpers ----
    void RestoreTTY() { LeaveRaw(); }

    Engine(){ InstallBuiltins(); }

    // Entrypoints
    void Eval(const ::Source& S){ src = &S; ExecRange(0, src->text.size()); }
    void Eval(const std::string& program){ ::Source S{"<memory>", program}; Eval(S); }
    void eval(const ::Source& S){ Eval(S); } // alias

private:
    // ---------- builtins ----------
    static void Require(const std::vector<Value>& a, size_t n, const char* name){
        if (a.size()!=n){ std::ostringstream os; os<<name<<" expects "<<n<<" args"; throw ScriptError(os.str(), Span{}); }
    }
    void AddVoid(const std::string& n, std::function<void(const std::vector<Value>&)> fn){
        fns[n] = [fn](const std::vector<Value>& a)->std::optional<Value>{ fn(a); return std::nullopt; };
    }
    void AddRet (const std::string& n, std::function<Value(const std::vector<Value>&)> fn){
        fns[n] = [fn](const std::vector<Value>& a)->std::optional<Value>{ return fn(a); };
    }

    void InstallBuiltins(){
        // screen / text
        AddVoid("pos",   [this](auto& a){ Require(a,2,"pos");   std::cout << "\x1b[" << a[1].AsInt() << ";" << a[0].AsInt() << "H"; if(!silentIO) std::cout.flush(); });
        AddVoid("color", [this](auto& a){ Require(a,1,"color"); std::cout << "\x1b[" << a[0].AsInt() << "m"; if(!silentIO) std::cout.flush(); });
        AddVoid("print", [this](auto& a){
            Require(a,1,"print");
            if (!silentIO){
                std::string s = a[0].AsStr();
                for (char& ch : s) if (ch=='\t') ch=' ';
                std::string norm; norm.reserve(s.size()*2);
                for (char ch : s){
#ifndef _WIN32
                    if (haveOld && ch=='\n'){ norm.push_back('\r'); norm.push_back('\n'); }
                    else                      { norm.push_back(ch); }
#else
                    if (ch=='\n'){ norm.push_back('\r'); norm.push_back('\n'); }
                    else          { norm.push_back(ch); }
#endif
                }
                std::cout << norm;
            }
        });

        // timing & math & util
        AddVoid("sleep",   [](auto& a){ Require(a,1,"sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(a[0].AsInt())); });
        AddRet ("RandInt", [](auto& a){ Require(a,2,"RandInt"); int lo=(int)a[0].AsInt(), hi=(int)a[1].AsInt(); if(lo>hi) std::swap(lo,hi); static std::mt19937 rng(std::random_device{}()); std::uniform_int_distribution<int> d(lo,hi); return Value::MakeInt(d(rng)); });
        AddRet ("abs",     [](auto& a){ Require(a,1,"abs"); return Value::MakeInt(std::llabs(a[0].AsInt())); });
        AddRet ("len",     [](auto& a){ Require(a,1,"len"); if(a[0].t==Type::Str) return Value::MakeInt((long long)a[0].AsStr().size()); if(a[0].t==Type::List) return Value::MakeInt((long long)a[0].AsList().size()); throw ScriptError("Len expects str or list", Span{}); });
        AddVoid("cls",     [this](auto&a){Require(a,0,"cls"); if(!silentIO) std::cout << "\x1b[2J\x1b[H";});

        // files
        AddRet("Load", [](auto& a){
            Require(a,1,"Load"); const std::string path=a[0].AsStr();
            std::ifstream in(path, std::ios::binary);
            if (!in) return Value::MakeStr(std::string{});
            std::string data; in.seekg(0,std::ios::end); std::streampos sz=in.tellg();
            if (sz>0){ data.resize((size_t)sz); in.seekg(0,std::ios::beg); in.read(&data[0], data.size()); }
            return Value::MakeStr(data);
        });
        AddRet("Save", [](auto& a){
            Require(a,2,"Save"); const std::string path=a[0].AsStr(); const std::string data=a[1].AsStr();
            std::ofstream out(path, std::ios::binary|std::ios::trunc);
            if (!out) return Value::MakeBool(false);
            out.write(data.data(), (std::streamsize)data.size());
            return Value::MakeBool((bool)out);
        });

        // lists
        AddRet ("ListGet",  [](auto& a){
            Require(a,2,"ListGet");
            if(a[0].t!=Type::List) throw ScriptError("ListGet expects list", Span{});
            const auto& xs = a[0].AsList(); long long idxll=a[1].AsInt(); if(idxll<0) return Value::MakeInt(0);
            size_t idx=(size_t)idxll; if(idx>=xs.size()) return Value::MakeInt(0); return xs[idx];
        });
        AddVoid("ListSet",  [](auto& a){
            Require(a,3,"ListSet");
            if(a[0].t!=Type::List) throw ScriptError("ListSet expects list", Span{});
            auto& xs = const_cast<std::vector<Value>&>(a[0].AsList());
            size_t idx=(size_t)a[1].AsInt(); if(idx>=xs.size()) throw ScriptError("ListSet: index out of range", Span{});
            xs[idx]=a[2];
        });
        AddVoid("ListPush", [](auto& a){
            Require(a,2,"ListPush");
            if(a[0].t!=Type::List) throw ScriptError("ListPush expects list", Span{});
            auto& xs = const_cast<std::vector<Value>&>(a[0].AsList()); xs.push_back(a[1]);
        });

        // keys (Enter/BackSpace/Escape/ESC-seqs; avoid consuming mouse SGR)
        AddRet("Input.Key", [this](auto& a){
            Require(a,0,"Input.Key");
            if (!std::cin.good()) return Value::MakeStr(std::string{});
            int c = std::cin.get();
            if (c==EOF) return Value::MakeStr(std::string{});

            if (c=='\r' || c=='\n'){
                if (c=='\r' && std::cin.rdbuf()->in_avail()){
                    int pk = std::cin.peek(); if (pk=='\n') (void)std::cin.get();
                }
                return Value::MakeStr(std::string{"Enter"});
            }
            if (c==0x08 || c==0x7F) return Value::MakeStr(std::string{"BackSpace"});

            if (c==0x1B){
                auto* rb = std::cin.rdbuf();
                int p1 = rb->sgetc();
                if (p1==EOF) return Value::MakeStr(std::string{"Escape"});
                if (p1=='['){
                    rb->sbumpc();
                    int p2 = rb->sgetc();
                    rb->sungetc();
                    if (p2=='M' || p2=='<'){ std::cin.putback((char)0x1B); return Value::MakeStr(std::string{}); }
                }
                std::string s; s.push_back((char)0x1B);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                while (std::cin.rdbuf()->in_avail()){
                    int d = std::cin.get(); if (d==EOF) break;
                    s.push_back((char)d);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (s.size()==1) return Value::MakeStr(std::string{"Escape"});
                return Value::MakeStr(s);
            }

            std::string s; s.push_back((char)c);
            return Value::MakeStr(s);
        });

        // SGR mouse: ESC [ < b ; x ; y (M|m)
        AddRet("Input.Mouse", [this](auto& a){
            Require(a,0,"Input.Mouse");
            if (!std::cin.good()) return Value::MakeList({});
            int c1 = std::cin.peek();
            if (c1 != 0x1B) return Value::MakeList({});
            std::cin.get(); // ESC
            if (std::cin.peek()!='['){ std::cin.putback((char)0x1B); return Value::MakeList({}); }
            std::cin.get(); // [
            if (std::cin.peek()!='<'){ std::cin.putback('['); std::cin.putback((char)0x1B); return Value::MakeList({}); }
            std::cin.get(); // <

            auto readInt = [] (int& out)->bool {
                std::string num;
                while (std::cin.good()){
                    int p = std::cin.peek(); if (p<0 || !std::isdigit((unsigned char)p)) break;
                    num.push_back((char)std::cin.get());
                }
                if (num.empty()) return false; out = std::stoi(num); return true;
            };

            int b=0,x=0,y=0;
            if (!readInt(b)) return Value::MakeList({});
            if (std::cin.peek()==';') std::cin.get(); else return Value::MakeList({});
            if (!readInt(x)) return Value::MakeList({});
            if (std::cin.peek()==';') std::cin.get(); else return Value::MakeList({});
            if (!readInt(y)) return Value::MakeList({});
            int term = std::cin.get();
            if (term!='M' && term!='m') return Value::MakeList({});

            std::string kind = (b & 32) ? "drag" : ((term=='M') ? "down" : "up");
            int button = (b & 3) + 1;
            return Value::MakeList({ Value::MakeStr(kind), Value::MakeInt(button), Value::MakeInt(x), Value::MakeInt(y) });
        });

        AddVoid("Input.Start", [this](auto& a){
            Require(a, 0, "Input.Start");
            EnterRaw();
        });

        AddVoid("Input.Stop", [this](auto& a){
            Require(a, 0, "Input.Stop");
            LeaveRaw();
        });

        AddVoid("Input.EnableMouse", [this](auto& a){
            Require(a, 0, "Input.EnableMouse");
            std::cout << "\x1b[?1000h\x1b[?1006h"; // X10 + SGR mouse
            if (!silentIO) std::cout.flush();
        });

        AddVoid("Input.DisableMouse", [this](auto& a){
            Require(a, 0, "Input.DisableMouse");
            std::cout << "\x1b[?1000l\x1b[?1006l";
            if (!silentIO) std::cout.flush();
        });


        // size utility
        AddVoid("Size", [this](auto& a){
            Require(a,2,"Size");
            int rows=(int)a[0].AsInt(), cols=(int)a[1].AsInt();
            bool ok = SetTerminalSizeNow(rows, cols);
            if (!silentIO){ std::cout << "\x1b[2J\x1b[H"; if (!ok) std::cout << "\x1b["<<rows<<";1H" << "[Note] Could not enforce exact size."; std::cout.flush(); }
        });
    }

    // ---------- exec ----------
    struct ReturnSignal     : std::exception { Value val; explicit ReturnSignal(Value v): val(std::move(v)) {} };
    struct VoidReturnSignal : std::exception {};

    size_t FindMatchingBrace(size_t openPos) const {
        const std::string& s = src->text;
        size_t n = s.size(); int depth = 0;
        for (size_t i = openPos; i < n; ++i){
            char c = s[i];
            if (c=='"' || c=='\''){ char q=c; ++i; while(i<n){ if (s[i]=='\\'){ i+=2; continue; } if(s[i]==q){ break; } ++i; } continue; }
            if (c=='/' && i+1<n && s[i+1]=='/'){ i+=2; while(i<n && s[i]!='\n') ++i; continue; }
            if (c=='/' && i+1<n && s[i+1]=='*'){ i+=2; while(i+1<n && !(s[i]=='*'&&s[i+1]=='/')) ++i; if(i+1<n) ++i; continue; }
            if (c=='{') ++depth;
            else if (c=='}'){ if (--depth==0) return i; }
        }
        throw ScriptError("unterminated block", Span{});
    }

    void ExecRange(size_t begin, size_t end){
        Pos P; P.i = begin; P.src = &src->text;

        auto parseType = [](Pos& p)->Type{
            SkipWS(p);
            if (StartsWithKW(p,"int"))   { p.i+=3; return Type::Int; }
            if (StartsWithKW(p,"float")) { p.i+=5; return Type::Float; }
            if (StartsWithKW(p,"bool"))  { p.i+=4; return Type::Bool; }
            if (StartsWithKW(p,"str"))   { p.i+=3; return Type::Str;  }
            if (StartsWithKW(p,"list"))  { p.i+=4; return Type::List; }
            throw ScriptError("unknown type (use int|float|bool|str|list)", Span{});
        };

        while (true){
            SkipWS(P);
            if (P.i >= end) break;

            if (StartsWithKW(P, "exit")){
                P.i += 4; SkipWS(P);
                Expect(P,';');
                std::cout << "\x1b[0m";
                throw std::runtime_error("exit called");
            }

            // import "x"; or import x;
            if (StartsWithKW(P,"import")){
                P.i += 6; SkipWS(P);
                if (!AtEnd(P) && ((*P.src)[P.i]=='"' || (*P.src)[P.i]=='\'')) (void)ParseQuoted(P);
                else (void)ParseIdent(P);
                Expect(P,';');
                continue;
            }

            if (StartsWithKW(P, "del")){
                P.i += 3;
                SkipWS(P);
                std::string name = ParseIdent(P);
                Expect(P, ';');

                if (!cur->Erase(name)) {
                    throw ScriptError("del: unknown variable: " + name, Span{});
                }
                continue;
            }

            // return [expr] ;
            if (StartsWithKW(P,"return")){
                P.i += 6;
                SkipWS(P);
                if (Match(P,';')){              // bare 'return;'
                    throw VoidReturnSignal{};
                }
                Parser ex(P, cur, src, &fns);
                Value v = ex.ParseExpr();
                Expect(P,';');
                throw ReturnSignal(std::move(v));
            }

            /// func <ret-type> <name>(...) { ... }
            // func <name>(...) { ... }
            if (StartsWithKW(P,"func")){
                P.i += 4;
                SkipWS(P);

                auto nextIsType = [&](Pos& pp)->bool{
                    Pos t = pp; SkipWS(t);
                    return StartsWithKW(t,"void") || StartsWithKW(t,"int") || StartsWithKW(t,"float")
                        || StartsWithKW(t,"bool") || StartsWithKW(t,"str") || StartsWithKW(t,"list");
                };

                bool isVoid = false;
                Type rt = Type::Int;
                bool hasExplicitRet = false;
                std::string fname;

                if (nextIsType(P)){
                    if (StartsWithKW(P,"void")) { P.i += 4; isVoid = true; }
                    else { rt = parseType(P); }
                    hasExplicitRet = !isVoid; // only matters for non-void

                    SkipWS(P);
                    if (AtEnd(P) || !IsIdentStart((*P.src)[P.i]))
                        throw ScriptError("expected function name after return type (anonymous functions not supported here)", Span{P.i, P.i+1});
                    fname = ParseIdent(P);
                } else {
                    // LEGACY: name first
                    fname = ParseIdent(P);
                    // isVoid=false; rt=Int default; hasExplicitRet=false
                }

                SkipWS(P); Expect(P,'(');
                std::vector<std::string> params;
                SkipWS(P);
                if (!Match(P,')')){
                    while (true){
                        params.push_back(ParseIdent(P));
                        SkipWS(P);
                        if (Match(P,')')) break;
                        Expect(P, ',');
                    }
                }
                SkipWS(P); Expect(P,'{');
                size_t o = P.i - 1;
                size_t c = FindMatchingBrace(o);
                size_t bodyBeg = o + 1;
                size_t after   = c + 1;

                UFunc F{fname, params, bodyBeg, c};
                F.isVoid = isVoid;
                F.retType = rt;
                F.hasExplicitRet = hasExplicitRet;

                ufns[fname] = F;
                if (isVoid){
                    fns[fname] = [this, fname](const std::vector<Value>& a)->std::optional<Value>{
                        const auto& UF = ufns.at(fname);
                        CallUserVoid(UF, a);
                        return std::nullopt;
                    };
                } else {
                    fns[fname] = [this, fname](const std::vector<Value>& a)->std::optional<Value>{
                        const auto& UF = ufns.at(fname);
                        return CallUser(UF, a);
                    };
                }

                P.i = after;
                continue;
            }



            // while (cond) { ... }
            if (StartsWithKW(P,"while")){
                P.i += 5; SkipWS(P);
                Expect(P,'(');
                size_t condStart = P.i;
                { Parser cfirst(P, cur, src, &fns); (void)cfirst.ParseExpr(); Expect(P,')'); }
                SkipWS(P); Expect(P,'{');
                size_t openPos  = P.i - 1;
                size_t closePos = FindMatchingBrace(openPos);
                size_t bodyBeg  = openPos + 1;
                size_t afterBlk = closePos + 1;

                for (;;){
                    Pos CP; CP.i = condStart; CP.src = &src->text;
                    Parser cpar(CP, cur, src, &fns);
                    if (!cpar.ParseExpr().AsBool()) break;
                    ExecRange(bodyBeg, closePos);
                }
                P.i = afterBlk;
                continue;
            }

            // if (...) { ... } [elif (...) { ... }]* [else { ... }]
            if (StartsWithKW(P,"if")){
                P.i += 2; SkipWS(P);
                Expect(P,'(');
                Parser cpar(P, cur, src, &fns);
                bool cond0 = cpar.ParseExpr().AsBool();
                Expect(P,')');
                SkipWS(P); Expect(P,'{');
                size_t tOpen = P.i - 1;
                size_t tClose= FindMatchingBrace(tOpen);
                size_t tBeg  = tOpen + 1;
                size_t after = tClose + 1;

                struct ElIf { size_t beg, end; bool cond; };
                std::vector<ElIf> elifs;

                while (true){
                    Pos peek; peek.i = after; peek.src = &src->text;
                    SkipWS(peek);
                    if (!StartsWithKW(peek,"elif")) break;
                    P.i = peek.i + 4; SkipWS(P); Expect(P,'(');
                    Parser epar(P, cur, src, &fns);
                    bool econd = epar.ParseExpr().AsBool();
                    Expect(P,')'); SkipWS(P); Expect(P,'{');
                    size_t eOpen = P.i - 1;
                    size_t eClose= FindMatchingBrace(eOpen);
                    elifs.push_back(ElIf{ eOpen+1, eClose, econd });
                    after = eClose + 1;
                }

                bool hasElse=false; size_t eBeg=0,eEnd=0; size_t afterElse=after;
                {   Pos peek; peek.i = after; peek.src = &src->text; SkipWS(peek);
                    if (StartsWithKW(peek,"else")){
                        hasElse=true; P.i = peek.i + 4; SkipWS(P); Expect(P,'{');
                        size_t o = P.i - 1; size_t c = FindMatchingBrace(o);
                        eBeg = o + 1; eEnd = c; afterElse = c + 1;
                    } else { P.i = after; }
                }

                bool ran=false;
                if (cond0){ ExecRange(tBeg,tClose); ran=true; }
                else {
                    for (auto& e : elifs){ if (e.cond){ ExecRange(e.beg,e.end); ran=true; break; } }
                }
                if (!ran && hasElse) ExecRange(eBeg,eEnd);

                P.i = hasElse ? afterElse : after;
                continue;
            }

            // let [type|auto] name = expr ;
            if (StartsWithKW(P,"let")){
                P.i += 3; SkipWS(P);
                bool isAuto=false; Type T=Type::Int;
                if (StartsWithKW(P,"auto")) { P.i += 4; isAuto=true; }
                else                         { T = parseType(P); }
                SkipWS(P);
                if (Match(P, ':')) { /* allow optional ':' before name */ }
                std::string name = ParseIdent(P);
                SkipWS(P); Expect(P,'=');
                Parser ex(P, cur, src, &fns); Value v = ex.ParseExpr();
                Expect(P,';');
                if (isAuto){
                    if (cur->Exists(name)) cur->Set(name, v); else cur->Declare(name, v.t, v);
                } else {
                    if (cur->Exists(name)){
                        if (cur->Get(name).declared != T) throw ScriptError("variable already declared with different type: " + name, Span{});
                        cur->Set(name, v);
                    } else {
                        cur->Declare(name, T, v);
                    }
                }
                continue;
            }

            // Plain assignment or call
            if (!AtEnd(P) && IsIdentStart((*P.src)[P.i])){
                size_t save = P.i;
                std::string name = ParseIdent(P);
                SkipWS(P);
                if (!AtEnd(P) && (*P.src)[P.i]=='='){
                    ++P.i;
                    Parser ex(P, cur, src, &fns); Value v = ex.ParseExpr();
                    Expect(P,';');
                    cur->SetOrDeclare(name, v);
                    continue;
                } else {
                    // function call statement
                    P.i = save;
                    std::string fname = ParseIdent(P);
                    Expect(P,'(');
                    std::vector<Value> args;
                    SkipWS(P);
                    if (!Match(P,')')){
                        while (true){
                            Parser ex(P, cur, src, &fns); Value v = ex.ParseExpr();
                            args.push_back(v);
                            SkipWS(P);
                            if (Match(P,')')) break;
                            Expect(P, ',');
                        }
                    }
                    size_t callEnd = P.i;
                    Span callSpan{save, callEnd};
                    auto it = fns.find(fname);
                    if (it==fns.end()) throw ScriptError("unknown function: " + fname, callSpan);
                    try { (void)it->second(args); }
                    catch (ScriptError& se){ if (se.span.beg==0&&se.span.end==0) se.span=callSpan; se.notes.push_back("in call to '"+fname+"'"); throw; }
                    catch (std::exception& se){ ScriptError e(std::string("runtime: ")+se.what(), callSpan); e.notes.push_back("in call to '"+fname+"'"); throw e; }
                    Expect(P,';');
                    continue;
                }
            }

            // nothing matched
            throw ScriptError("unexpected token", Span{P.i,P.i+1});
        }
    }

    // ---------- calls ----------
    Value CallUser(const UFunc& F, const std::vector<Value>& args){
        if (args.size() != F.params.size()){
            std::ostringstream os; os << "wrong number of arguments for " << F.name
                                    << " (expected " << F.params.size() << ", got " << args.size() << ")";
            throw ScriptError(os.str(), Span{});
        }
        Env local(cur);
        Env* saved = cur;
        cur = &local;

        for (size_t i=0;i<args.size();++i) cur->Declare(F.params[i], args[i].t, args[i]);

        try {
            ExecRange(F.bodyBeg, F.bodyEnd);
            cur = saved;
            throw ScriptError("function returns no value", Span{});
        } catch (VoidReturnSignal&){
            cur = saved;
            throw ScriptError("non-void function used 'return;' without a value", Span{});
        } catch (ReturnSignal& rs){
            cur = saved;
            if (F.isVoid) throw ScriptError("internal: void flag mismatch", Span{});

            if (!F.hasExplicitRet) return rs.val;

            if (rs.val.t != F.retType){
                auto num = [](Type t){ return t==Type::Int||t==Type::Float||t==Type::Bool; };
                if (num(rs.val.t) && num(F.retType)){
                    if (F.retType==Type::Int)   return Value::MakeInt  (rs.val.AsInt());
                    if (F.retType==Type::Float) return Value::MakeFloat(rs.val.AsFloat());
                    if (F.retType==Type::Bool)  return Value::MakeBool (rs.val.AsBool());
                }
                throw ScriptError("return type mismatch", Span{});
            }
            return rs.val;
        } catch (...) {
            cur = saved;
            throw;
        }
    }
    void CallUserVoid(const UFunc& F, const std::vector<Value>& args){
        if (args.size() != F.params.size()){
            std::ostringstream os; os << "wrong number of arguments for " << F.name
                                    << " (expected " << F.params.size() << ", got " << args.size() << ")";
            throw ScriptError(os.str(), Span{});
        }
        Env local(cur);
        Env* saved = cur;
        cur = &local;

        for (size_t i=0;i<args.size();++i) cur->Declare(F.params[i], args[i].t, args[i]);

        try {
            ExecRange(F.bodyBeg, F.bodyEnd);
            cur = saved;               // ran to end: OK for void
            return;
        } catch (ReturnSignal& rs){
            cur = saved;
            throw ScriptError("void function returned a value", Span{});
        } catch (VoidReturnSignal&){
            cur = saved;               // explicit 'return;' : OK for void
            return;
        } catch (...) {
            cur = saved;
            throw;
        }
    }

    // ---------- raw mode ----------
    void EnterRaw(){
#ifdef _WIN32
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(hin,  &oldIn);
        GetConsoleMode(hout, &oldOut);
        DWORD inMode = oldIn;
        inMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
        inMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(hin, inMode);
        DWORD outMode = oldOut;
        outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hout, outMode);
#else
        termios tio{};
        if (tcgetattr(STDIN_FILENO, &oldTio)==0){
            haveOld = true;
            tio = oldTio;
            cfmakeraw(&tio);
            tcsetattr(STDIN_FILENO, TCSANOW, &tio);
        }
#endif
    }
    void LeaveRaw(){
#ifdef _WIN32
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (oldIn)  SetConsoleMode(hin,  oldIn);
        if (oldOut) SetConsoleMode(hout, oldOut);
#else
        if (haveOld) tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
#endif
    }
    static bool SetTerminalSizeNow(int rows, int cols){
#ifdef _WIN32
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hout == INVALID_HANDLE_VALUE) return false;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(hout, &info)) return false;
        COORD buf{ (SHORT)std::max<int>(cols, info.dwSize.X),
                   (SHORT)std::max<int>(rows, info.dwSize.Y) };
        if (!SetConsoleScreenBufferSize(hout, buf)) {
            buf.X = (SHORT)std::max<int>(cols, buf.X + 20);
            buf.Y = (SHORT)std::max<int>(rows, buf.Y + 200);
            if (!SetConsoleScreenBufferSize(hout, buf)) return false;
        }
        SMALL_RECT rect{0,0,(SHORT)(cols-1),(SHORT)(rows-1)};
        if (!SetConsoleWindowInfo(hout, TRUE, &rect)) return false;
        return true;
#else
#ifdef TIOCSWINSZ
        winsize ws{}; ws.ws_row=(unsigned short)rows; ws.ws_col=(unsigned short)cols;
        (void)ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);
#endif
        std::cout << "\x1b[8;"<<rows<<";"<<cols<<"t"; std::cout.flush();
        return true;
#endif
    }
};

} // namespace minis
