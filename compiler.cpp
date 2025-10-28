inline static void write_u8 (FILE*f, uint8_t  v){ fwrite(&v,1,1,f); }
inline static void write_u64(FILE*f, uint64_t v){ fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64_t  v){ fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const std::string&s){ uint64_t n=s.size(); write_u64(f,n); if(n) fwrite(s.data(),1,n,f); }

// inline static uint8_t  read_u8 (FILE*f){ uint8_t v;  fread(&v,1,1,f); return v; }
// inline static uint64_t read_u64(FILE*f){ uint64_t v; fread(&v,8,1,f); return v; }
// inline static std::string read_str(FILE*f){ uint64_t n=read_u64(f); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); return s; }

struct Compiler {
  Pos P;
  FILE* out=nullptr;
  std::vector<FnInfo> fns;
  std::unordered_map<std::string,size_t> fnIndex;
  bool inWith;

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