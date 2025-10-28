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