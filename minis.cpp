// minis.cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
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

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

#include "diagnostics.h"

// -----------------------------------------------
// Minimal helpers
// -----------------------------------------------
namespace minis {

struct Pos {
    size_t i = 0;
    const std::string* src = nullptr;
};

static inline bool AtEnd(Pos& p){ return p.i >= p.src->size(); }

static inline bool IsIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_'; }
static inline bool IsIdentCont(char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='.'; }

static inline void SkipWS(Pos& p){
    const std::string& s = *p.src;
    while (true){
        // ASCII whitespace
        while (!AtEnd(p) && std::isspace((unsigned char)s[p.i])) ++p.i;

        // line comment: //
        if (!AtEnd(p) && p.i+1<s.size() && s[p.i]=='/' && s[p.i+1]=='/'){
            p.i += 2; while (!AtEnd(p) && s[p.i] != '\n') ++p.i;
            continue;
        }
        // block comment: /* ... */
        if (!AtEnd(p) && p.i+1<s.size() && s[p.i]=='/' && s[p.i+1]=='*'){
            p.i += 2; while (p.i+1<s.size() && !(s[p.i]=='*' && s[p.i+1]=='/')) ++p.i;
            if (p.i+1<s.size()) p.i += 2;  // eat closing */
            continue;
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
    if (p.i + L <= p.src->size() && p.src->compare(p.i, L, s) == 0){
        p.i += L;
        return true;
    }
    return false;
}

static inline void Expect(Pos& p, char c){
    SkipWS(p);
    size_t where = p.i;
    if (AtEnd(p) || (*p.src)[p.i] != c){
        throw ScriptError(std::string("expected '") + c + "'", Span{where, where+1});
    }
    ++p.i; // consume
}

static inline std::string ParseIdent(Pos& p){
    SkipWS(p);
    size_t s = p.i;
    if (AtEnd(p) || !IsIdentStart((*p.src)[p.i])) throw ScriptError("expected identifier", Span{s,s+1});
    ++p.i;
    while (!AtEnd(p) && IsIdentCont((*p.src)[p.i])) ++p.i;
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
    size_t s = p.i;
    size_t L = std::strlen(kw);
    if (s+L > p.src->size()) return false;
    if (p.src->compare(s, L, kw) != 0) return false;
    auto iscont = [](char c){ return std::isalnum((unsigned char)c) || c=='_'; };
    bool leftOk  = (s==0) || !iscont((*p.src)[s-1]);
    bool rightOk = (s+L>=p.src->size()) || !iscont((*p.src)[s+L]);
    return leftOk && rightOk;
}

// -----------------------------------------------
// Values / Types
// -----------------------------------------------
enum class Type { Int, Float, Bool, Str, List };

struct Value {
    Type t;
    std::variant<long long,double,bool,std::string,std::vector<Value>> v;

    static Value MakeInt(long long x){ return {Type::Int, x}; }
    static Value MakeFloat(double x){ return {Type::Float, x}; }
    static Value MakeBool(bool x){ return {Type::Bool, x}; }
    static Value MakeStr(std::string s){ return {Type::Str, std::move(s)}; }
    static Value MakeList(std::vector<Value> xs){ return {Type::List, std::move(xs)}; }

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
    const std::vector<Value>& AsList() const {
        if (t!=Type::List) throw ScriptError("expected list", Span{});
        return std::get<std::vector<Value>>(v);
    }
    std::vector<Value>& AsList() {
        if (t!=Type::List) throw ScriptError("expected list", Span{});
        return std::get<std::vector<Value>>(v);
    }
};
// Add right after struct Value { ... }; inside namespace minis

inline bool operator==(const Value& a, const Value& b){
    if (a.t != b.t) return false;
    switch (a.t){
        case Type::Int:   return std::get<long long>(a.v) == std::get<long long>(b.v);
        case Type::Float: return std::get<double>(a.v)    == std::get<double>(b.v);
        case Type::Bool:  return std::get<bool>(a.v)      == std::get<bool>(b.v);
        case Type::Str:   return std::get<std::string>(a.v)
                                == std::get<std::string>(b.v);
        case Type::List:  return std::get<std::vector<Value>>(a.v)
                                == std::get<std::vector<Value>>(b.v); // recursive
    }
    return false; // unreachable, keeps compilers happy
}

inline bool operator!=(const Value& a, const Value& b){
    return !(a == b);
}

struct Var { Type declared; Value val; };

class Env {
    std::unordered_map<std::string, Var> m;
public:
    bool Exists(const std::string& n) const { return m.find(n)!=m.end(); }
    const Var& Get(const std::string& n) const {
        auto it=m.find(n); if(it==m.end()) throw ScriptError("unknown variable: " + n, Span{});
        return it->second;
    }
    void Declare(const std::string& n, Type t, Value v){
        if (m.count(n)) throw ScriptError("variable already declared: " + n, Span{});
        AssignChecked(t, v);
        m.emplace(n, Var{t, v});
    }
    void Set(const std::string& n, Value v){
        auto it=m.find(n); if(it==m.end()) throw ScriptError("unknown variable: " + n, Span{});
        AssignChecked(it->second.declared, v);
        it->second.val = v;
    }
    std::vector<std::string> Names() const {
        std::vector<std::string> out; out.reserve(m.size());
        for (auto& kv : m) out.push_back(kv.first);
        return out;
    }
private:
    static void AssignChecked(Type t, Value& v){
        if (t==v.t) return;
        switch(t){
            case Type::Int:   v = Value::MakeInt(v.AsInt()); break;
            case Type::Float: v = Value::MakeFloat(v.AsFloat()); break;
            case Type::Bool:  v = Value::MakeBool(v.AsBool()); break;
            case Type::Str:
                if (v.t!=Type::Str)  throw ScriptError("cannot assign non-str to str", Span{});
                break;
            case Type::List:
                if (v.t!=Type::List) throw ScriptError("cannot assign non-list to list", Span{});
                break;
        }
    }
};

// -----------------------------------------------
// Parser
// -----------------------------------------------
class Parser {
public:
    Pos& p; Env* env; const ::Source* src;
    std::unordered_map<std::string, std::function<std::optional<Value>(const std::vector<Value>&)>>* fnmap;

    Parser(Pos& P, Env* E, const ::Source* S,
           std::unordered_map<std::string, std::function<std::optional<Value>(const std::vector<Value>&)>>* F)
        : p(P), env(E), src(S), fnmap(F) {}

    Value ParseExpr(){ return ParseEquality(); }

private:
    Value ParseEquality(){
        Value v = ParseAddSub();
        while (true){
            if (MatchStr(p, "==")){
                Value r = ParseAddSub();
                bool eq = false;
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
                    eq = (isNumL && isNumR) ? (v.AsFloat() == r.AsFloat()) : false;
                }
                v = Value::MakeBool(eq);
            } else if (MatchStr(p, "!=")){
                Value r = ParseAddSub();
                bool ne = true;
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
                    ne = (isNumL && isNumR) ? (v.AsFloat() != r.AsFloat()) : true;
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
        if (!AtEnd(p) && ((*p.src)[p.i]=='"' || (*p.src)[p.i]=='\'')){ return Value::MakeStr(ParseQuoted(p)); }
        // bool literals
        if (p.i+4<=p.src->size() && p.src->compare(p.i,4,"true")==0 && (p.i+4==p.src->size() || !IsIdentCont((*p.src)[p.i+4]))){ p.i+=4; return Value::MakeBool(true); }
        if (p.i+5<=p.src->size() && p.src->compare(p.i,5,"false")==0 && (p.i+5==p.src->size() || !IsIdentCont((*p.src)[p.i+5]))){ p.i+=5; return Value::MakeBool(false); }
        // list literal
        if (!AtEnd(p) && (*p.src)[p.i]=='['){ ++p.i; return ParseListLit(); }
        // number
        if (!AtEnd(p) && (std::isdigit((unsigned char)(*p.src)[p.i]) || (*p.src)[p.i]=='+' || (*p.src)[p.i]=='-')){
            auto s = ParseNumberText(p);
            if (s.find('.')!=std::string::npos) return Value::MakeFloat(std::stod(s));
            return Value::MakeInt(std::stoll(s));
        }
        // identifier or function call
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

// -----------------------------------------------
// Engine
// -----------------------------------------------
struct Frame { std::string fn; Span call; };

class Engine {
public:
    using Handler = std::function<std::optional<Value>(const std::vector<Value>&)>;
    std::unordered_map<std::string, Handler> fns;
    Env env;
    bool silentIO = false;

    const ::Source* src = nullptr;
    std::vector<Frame> stack;

#ifdef _WIN32
    DWORD oldIn = 0, oldOut = 0;
#else
    termios oldTio{};
    bool haveOld = false;
#endif

    Engine(){
        // ------------ Built-ins ------------
        AddVoid("pos",   [this](auto& a){ Require(a,2,"pos");   std::cout << "\x1b[" << a[1].AsInt() << ";" << a[0].AsInt() << "H"; if(!silentIO) std::cout.flush(); });
        AddVoid("color", [this](auto& a){ Require(a,1,"color"); std::cout << "\x1b[" << a[0].AsInt() << "m"; if(!silentIO) std::cout.flush(); });
        AddVoid("print", [this](auto& a){ Require(a,1,"print"); if(!silentIO) std::cout << a[0].AsStr(); });
        AddVoid("cls",   [this](auto& a){ Require(a,0,"cls");   if(!silentIO) std::cout << "\x1b[2J\x1b[H"; });

        AddVoid("sleep",   [](auto& a){ Require(a,1,"sleep"); std::this_thread::sleep_for(std::chrono::milliseconds(a[0].AsInt())); });
        AddRet ("RandInt", [](auto& a){ Require(a,2,"RandInt"); int lo=(int)a[0].AsInt(), hi=(int)a[1].AsInt(); if(lo>hi) std::swap(lo,hi); static std::mt19937 rng(std::random_device{}()); std::uniform_int_distribution<int> d(lo,hi); return Value::MakeInt(d(rng)); });
        AddRet ("abs",[](auto& a){ Require(a,1,"abs"); return Value::MakeInt(std::llabs(a[0].AsInt())); });
        AddRet ("timeMs",  [](auto&   ){ auto now=std::chrono::steady_clock::now(); auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(); return Value::MakeInt(ms); });
        AddRet ("len",     [](auto& a){ Require(a,1,"len"); if(a[0].t==Type::Str) return Value::MakeInt((long long)a[0].AsStr().size()); if(a[0].t==Type::List) return Value::MakeInt((long long)a[0].AsList().size()); throw ScriptError("Len expects str or list", Span{}); });
        AddVoid("Exit",    [this](auto& a){ Require(a,0,"Exit"); if(!silentIO) std::cout << "\x1b[0m"; throw std::runtime_error("exit called"); });

        // Lists (ListGet is bounds-safe: returns 0 when out of range)
        AddRet ("ListGet",  [](auto& a){
            Require(a,2,"ListGet");
            if(a[0].t!=Type::List) throw ScriptError("ListGet expects list", Span{});
            const auto& xs = a[0].AsList();
            long long idxll = a[1].AsInt();
            if (idxll < 0) return Value::MakeInt(0);
            size_t idx = (size_t)idxll;
            if (idx >= xs.size()) return Value::MakeInt(0);
            return xs[idx];
        });
        AddVoid("ListSet",  [](auto& a){
            Require(a,3,"ListSet");
            if(a[0].t!=Type::List) throw ScriptError("ListSet expects list", Span{});
            auto& xs = const_cast<std::vector<Value>&>(a[0].AsList());
            auto idx = (size_t)a[1].AsInt();
            if (idx >= xs.size()) throw ScriptError("ListSet: index out of range", Span{});
            xs[idx]=a[2];
        });
        AddVoid("ListPush", [](auto& a){
            Require(a,2,"ListPush");
            if(a[0].t!=Type::List) throw ScriptError("ListPush expects list", Span{});
            auto& xs = const_cast<std::vector<Value>&>(a[0].AsList());
            xs.push_back(a[1]);
        });

        // Input.*
        AddVoid("Input.Start",        [this](auto& a){ Require(a,0,"Input.Start"); EnterRaw(); });
        AddVoid("Input.Stop",         [this](auto& a){ Require(a,0,"Input.Stop");  LeaveRaw(); });
        AddVoid("Input.EnableMouse",  [this](auto& a){ Require(a,0,"Input.EnableMouse"); std::cout << "\x1b[?1000h\x1b[?1006h"; if(!silentIO) std::cout.flush(); });
        AddVoid("Input.DisableMouse", [this](auto& a){ Require(a,0,"Input.DisableMouse"); std::cout << "\x1b[?1000l\x1b[?1006l"; if(!silentIO) std::cout.flush(); });

        AddRet ("Input.Key",          [this](auto& a){
            Require(a,0,"Input.Key");
            if (!std::cin.good()) return Value::MakeStr(std::string{});
            int c = std::cin.get();
            if (c==EOF) return Value::MakeStr(std::string{});
            std::string s; s.push_back((char)c);
            if (c==0x1B){
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                while (std::cin.rdbuf()->in_avail()){
                    int d = std::cin.get(); if(d==EOF) break;
                    s.push_back((char)d);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            return Value::MakeStr(s);
        });

        // Non-greedy SGR mouse parser: ESC [ < b ; x ; y (M|m)
        AddRet ("Input.Mouse",        [this](auto& a){
            Require(a,0,"Input.Mouse");
            if (!std::cin.good()) return Value::MakeList({});

            int c1 = std::cin.peek();
            if (c1 != 0x1B) return Value::MakeList({}); // no ESC -> not mouse

            // Tentatively consume ESC and check next two chars are "[<"
            std::cin.get(); // consume ESC
            int c2 = std::cin.peek();
            if (c2 != '['){
                std::cin.putback((char)0x1B);
                return Value::MakeList({});
            }
            std::cin.get(); // '['
            int c3 = std::cin.peek();
            if (c3 != '<'){
                // Not SGR mouse; put back '[' then ESC
                std::cin.putback('[');
                std::cin.putback((char)0x1B);
                return Value::MakeList({});
            }
            std::cin.get(); // '<' -> committed to mouse

            auto readInt = [] (int& out)->bool {
                std::string num;
                while (std::cin.good()){
                    int p = std::cin.peek();
                    if (p < 0 || !std::isdigit((unsigned char)p)) break;
                    num.push_back((char)std::cin.get());
                }
                if (num.empty()) return false;
                out = std::stoi(num);
                return true;
            };

            int b=0,x=0,y=0;
            if (!readInt(b)) return Value::MakeList({});
            if (std::cin.peek()==';') std::cin.get(); else return Value::MakeList({});
            if (!readInt(x)) return Value::MakeList({});
            if (std::cin.peek()==';') std::cin.get(); else return Value::MakeList({});
            if (!readInt(y)) return Value::MakeList({});

            int term = std::cin.get();           // 'M' = press/drag, 'm' = release
            if (term!='M' && term!='m') return Value::MakeList({});

            std::string kind = (b & 32) ? "drag" : ((term=='M') ? "down" : "up");
            int button = (b & 3) + 1;
            return Value::MakeList({
                Value::MakeStr(kind),
                Value::MakeInt(button),
                Value::MakeInt(x),
                Value::MakeInt(y)
            });
        });

        // Size(rows, cols) — proactively resize; never throws
        AddVoid("Size", [this](auto& a){
            Require(a,2,"Size");
            int rows = (int)a[0].AsInt();
            int cols = (int)a[1].AsInt();
            bool ok = SetTerminalSizeNow(rows, cols);
            if (!silentIO){
                std::cout << "\x1b[2J\x1b[H";
                if (!ok){
                    std::cout << "\x1b[" << rows << ";1H"
                              << "[Note] Could not enforce size exactly; try a terminal that supports resizing.";
                }
                std::cout.flush();
            }
        });
    }

    // === Public entrypoints ===
    void Eval(const ::Source& S){
        src = &S;
        ExecRange(0, src->text.size());      // unified executor
    }
    void Eval(const std::string& program){ ::Source S{"<memory>", program}; Eval(S); }
    // lowercase shim used by avocado.cpp: vm.eval(src);
    void eval(const ::Source& S){ Eval(S); }

private:
    // ---- executor helpers ----
    void ExecRange(size_t begin, size_t end){
        Pos P; P.i = begin; P.src = &src->text;

        auto parseType = [this](Pos& p)->Type{
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

            // import "x"; or import x;
            if (StartsWithKW(P,"import")){
                P.i += 6; SkipWS(P);
                if (!AtEnd(P) && ((*P.src)[P.i]=='"' || (*P.src)[P.i]=='\'')) (void)ParseQuoted(P);
                else (void)ParseIdent(P);
                Expect(P,';');
                continue;
            }

            // let [type|auto] name = expr ;   (allow optional colon)
            if (StartsWithKW(P,"let")){
                P.i += 3; SkipWS(P);
                bool isAuto = false; Type T = Type::Int;
                if (StartsWithKW(P,"auto")) { P.i += 4; isAuto = true; }
                else { T = parseType(P); }
                SkipWS(P);
                if (Match(P, ':')) { /* optional colon */ }
                std::string name = ParseIdent(P);
                SkipWS(P); Expect(P,'=');
                Parser ex(P, &env, src, &fns); Value v = ex.ParseExpr();
                Expect(P,';');
                if (isAuto){
                    if (env.Exists(name)) env.Set(name, v); else env.Declare(name, v.t, v);
                } else env.Declare(name, T, v);
                continue;
            }

            // auto name = expr ;   (no 'let')
            if (StartsWithKW(P,"auto")){
                P.i += 4; SkipWS(P);
                std::string name = ParseIdent(P);
                SkipWS(P); Expect(P,'=');
                Parser ex(P, &env, src, &fns); Value v = ex.ParseExpr();
                Expect(P,';');
                if (env.Exists(name)) env.Set(name, v); else env.Declare(name, v.t, v);
                continue;
            }

            // while (cond) { ... }
            if (StartsWithKW(P,"while")){
                P.i += 5; SkipWS(P);
                Expect(P,'(');
                size_t condStart = P.i;               // reparse each iteration from here
                { Parser cfirst(P, &env, src, &fns); (void)cfirst.ParseExpr(); Expect(P,')'); }
                SkipWS(P); Expect(P,'{');
                size_t openPos  = P.i - 1;
                size_t closePos = FindMatchingBrace(openPos);
                size_t bodyBeg  = openPos + 1;
                size_t afterBlk = closePos + 1;

                for (;;){
                    Pos CP; CP.i = condStart; CP.src = &src->text;
                    Parser cpar(CP, &env, src, &fns);
                    if (!cpar.ParseExpr().AsBool()) break;
                    ExecRange(bodyBeg, closePos);     // execute the body subrange
                }

                P.i = afterBlk;                       // continue after the block
                continue;
            }

            // if (cond) { ... } [else { ... }]
            if (StartsWithKW(P,"if")){
                P.i += 2; SkipWS(P);
                Expect(P,'(');
                Parser cpar(P, &env, src, &fns);
                bool takeThen = cpar.ParseExpr().AsBool();
                Expect(P,')');
                SkipWS(P); Expect(P,'{');
                size_t thenOpen  = P.i - 1;
                size_t thenClose = FindMatchingBrace(thenOpen);
                size_t thenBeg   = thenOpen + 1;
                size_t afterThen = thenClose + 1;

                // optional else
                Pos Peek; Peek.i = afterThen; Peek.src = &src->text;
                SkipWS(Peek);
                bool hasElse = StartsWithKW(Peek,"else");
                size_t elseBeg=0, elseClose=0, afterElse=afterThen;
                if (hasElse){
                    P.i = Peek.i + 4; // consume 'else'
                    SkipWS(P); Expect(P,'{');
                    size_t eOpen  = P.i - 1;
                    elseClose = FindMatchingBrace(eOpen);
                    elseBeg   = eOpen + 1;
                    afterElse = elseClose + 1;
                } else {
                    P.i = afterThen;
                }

                if (takeThen) ExecRange(thenBeg, thenClose);
                else if (hasElse) ExecRange(elseBeg, elseClose);

                P.i = hasElse ? afterElse : afterThen;
                continue;
            }

            // assignment or call
            if (!AtEnd(P) && IsIdentStart((*P.src)[P.i])){
                size_t save = P.i;
                std::string name = ParseIdent(P);
                SkipWS(P);
                if (!AtEnd(P) && (*P.src)[P.i]=='='){
                    ++P.i;
                    Parser ex(P, &env, src, &fns); Value v = ex.ParseExpr();
                    env.Set(name, v);
                    Expect(P,';');
                    continue;
                } else {
                    P.i = save;
                }
            }

            // function call statement
            {
                size_t callStart = P.i;
                std::string fname = ParseIdent(P);
                Expect(P,'(');
                std::vector<Value> args;
                SkipWS(P);
                if (!Match(P,')')){
                    while (true){
                        Parser ex(P, &env, src, &fns); Value v = ex.ParseExpr();
                        args.push_back(v);
                        SkipWS(P);
                        if (Match(P,')')) break;
                        Expect(P, ',');
                    }
                }
                size_t callEnd = P.i;
                Span callSpan{callStart, callEnd};

                auto it = fns.find(fname);
                if (it==fns.end()){
                    ScriptError e("unknown function: " + fname, Span{callStart, callEnd});
                    throw e;
                }
                try { (void)it->second(args); }
                catch (ScriptError& se){
                    if (se.span.beg==0 && se.span.end==0) se.span = callSpan;
                    se.notes.push_back("in call to '" + fname + "'"); throw;
                }
                catch (std::exception& se){
                    ScriptError e(std::string("runtime: ") + se.what(), callSpan);
                    e.notes.push_back("in call to '" + fname + "'"); throw e;
                }
                Expect(P,';');
            }
        }
    }

    size_t FindMatchingBrace(size_t openPos) const {
        const std::string& s = src->text;
        size_t n = s.size(); int depth = 0;
        for (size_t i = openPos; i < n; ++i){
            char c = s[i];
            // skip strings
            if (c=='"' || c=='\''){ char q=c; ++i; while(i<n){ if(s[i]=='\\'){ i+=2; continue; } if(s[i]==q){ break; } ++i; } continue; }
            // skip // comments
            if (c=='/' && i+1<n && s[i+1]=='/'){ i+=2; while(i<n && s[i]!='\n') ++i; continue; }
            // skip /* */ comments
            if (c=='/' && i+1<n && s[i+1]=='*'){ i+=2; while(i+1<n && !(s[i]=='*' && s[i+1]=='/')) ++i; if(i+1<n) ++i; continue; }
            if (c=='{') ++depth;
            else if (c=='}'){ if (--depth==0) return i; }
        }
        throw ScriptError("unterminated block", Span{});
    }

    // ----- misc helpers -----
    static void Require(const std::vector<Value>& a, size_t n, const char* name){
        if (a.size()!=n){
            std::ostringstream os; os << name << " expects " << n << " args";
            throw ScriptError(os.str(), Span{});
        }
    }
    void AddVoid(const std::string& n, std::function<void(const std::vector<Value>&)> fn){
        fns[n] = [fn](const std::vector<Value>& a)->std::optional<Value>{ fn(a); return std::nullopt; };
    }
    void AddRet(const std::string& n, std::function<Value(const std::vector<Value>&)> fn){
        fns[n] = [fn](const std::vector<Value>& a)->std::optional<Value>{ return fn(a); };
    }

    // ----- Raw mode + terminal size -----
    void EnterRaw(){
#ifdef _WIN32
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(hin, &oldIn);
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

        COORD buf;
        buf.X = (SHORT)std::max<int>(cols, info.dwSize.X);
        buf.Y = (SHORT)std::max<int>(rows, info.dwSize.Y);
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
        winsize ws{};
        ws.ws_row = (unsigned short)rows;
        ws.ws_col = (unsigned short)cols;
        bool ok = (ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws) == 0);
    #else
        bool ok = false;
    #endif
        std::cout << "\x1b[8;" << rows << ";" << cols << "t";
        std::cout.flush();
        return ok || true;
#endif
    }
};

} // namespace minis
