struct Lexer {
  const std::string* src; size_t i=0, n=0; std::vector<Token> out;
  explicit Lexer(const std::string& s){ src=&s; n=s.size(); run(); }

  void ws(){ while(i<n && std::isspace((unsigned char)(*src)[i])) ++i; }

  static bool isIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
  static bool isIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

  void run(){
    while(true){
      ws(); size_t s=i; if(i>=n){ out.push_back({TKind::Eof,i,i,""}); break; }

      // comments
      if(i+1<n && (*src)[i]=='/' && (*src)[i+1]=='/'){
        i+=2; while(i<n && (*src)[i]!='\n') ++i; continue;
      }
      if(i+1<n && (*src)[i]=='/' && (*src)[i+1]=='*'){
        i+=2; int d=1; while(i+1<n && d>0){
          if((*src)[i]=='/' && (*src)[i+1]=='*'){ d++; i+=2; }
          else if((*src)[i]=='*' && (*src)[i+1]=='/'){ d--; i+=2; }
          else ++i;
        } continue;
      }

      // strings
      if((*src)[i]=='"' || (*src)[i]=='\''){
        char q=(*src)[i++]; bool esc=false;
        while(i<n){
          char c=(*src)[i++]; if(esc){ esc=false; continue; }
          if(c=='\\'){ esc=true; continue; }
          if(c==q) break;
        }
        out.push_back({TKind::Str,s,i,src->substr(s,i-s)}); continue;
      }

      // numbers
      if (std::isdigit((unsigned char)(*src)[i]) ||
         (((*src)[i]=='+'||(*src)[i]=='-') && i+1<n && std::isdigit((unsigned char)(*src)[i+1]))){
        ++i; while(i<n && (std::isdigit((unsigned char)(*src)[i]) || (*src)[i]=='.')) ++i;
        out.push_back({TKind::Num,s,i,src->substr(s,i-s)}); continue;
      }

      // identifiers & keywords
      if (isIdStart((*src)[i])){
        ++i; while(i<n && isIdCont((*src)[i])) ++i;
        std::string t = src->substr(s,i-s);
        TKind k = keyword(t);
        out.push_back({k,s,i,std::move(t)}); continue;
      }

      // 2-char ops
      auto two = (i+1<n) ? (std::string()+(*src)[i]+(*src)[i+1]) : "";
      if (two=="&&"){ i+=2; out.push_back({TKind::AndAnd,s,i,"&&"}); continue; }
      if (two=="||"){ i+=2; out.push_back({TKind::OrOr,s,i,"||"}); continue; }
      if (two=="=="){ i+=2; out.push_back({TKind::EQEQ,s,i,"=="}); continue; }
      if (two=="!="){ i+=2; out.push_back({TKind::NE  ,s,i,"!="}); continue; }
      if (two=="<="){ i+=2; out.push_back({TKind::LE  ,s,i,"<="}); continue; }
      if (two==">="){ i+=2; out.push_back({TKind::GE  ,s,i,">="}); continue; }

      // 1-char
      char c=(*src)[i++];
      TKind k = TKind::Eof;
      switch(c){
        case '(': k=TKind::LParen; break; case ')': k=TKind::RParen; break;
        case '{': k=TKind::LBrace; break; case '}': k=TKind::RBrace; break;
        case '[': k=TKind::LBracket;break; case ']': k=TKind::RBracket;break;
        case ',': k=TKind::Comma;   break; case ';': k=TKind::Semicolon;break;
        case ':': k=TKind::Colon;   break; case '=': k=TKind::Equal;    break;
        case '+': k=TKind::Plus;    break; case '-': k=TKind::Minus;    break;
        case '*': k=TKind::Star;    break; case '/': k=TKind::Slash;    break;
        case '!': k=TKind::Bang;    break; case '<': k=TKind::LT;       break;
        case '>': k=TKind::GT;      break;
        default:
          DIAG(Diagnostic::Warning, s,i, std::string("unknown char '")+c+"'");
          continue;
      }
      out.push_back({k,s,i,src->substr(s,i-s)});
    }
  }

  static TKind keyword(const std::string& t){
    if (t=="func")return TKind::Kw_func; if (t=="let") return TKind::Kw_let;
    if (t=="if")return TKind::Kw_if; if (t=="elif")return TKind::Kw_elif; if (t=="else")return TKind::Kw_else;
    if (t=="while")return TKind::Kw_while; if (t=="return")return TKind::Kw_return;
    if (t=="break")return TKind::Kw_break; if (t=="continue")return TKind::Kw_continue;
    if (t=="del")return TKind::Kw_del; if (t=="conv")return TKind::Kw_conv; if (t=="exit")return TKind::Kw_exit;
    if (t=="try")return TKind::Kw_try; if (t=="except")return TKind::Kw_except; if (t=="finally")return TKind::Kw_finally;
    if (t=="lambda")return TKind::Kw_lambda; if (t=="with")return TKind::Kw_with; if (t=="and")return TKind::Kw_and;
    if (t=="inline")return TKind::Kw_inline; if (t=="tailcall")return TKind::Kw_tailcall; if (t=="void")return TKind::Kw_void;
    if (t=="true")return TKind::Kw_true; if (t=="false")return TKind::Kw_false; if (t=="null")return TKind::Kw_null;
    if (t=="const")return TKind::Kw_const; if (t=="static")return TKind::Kw_static;
    if (t=="int")return TKind::Kw_int; if (t=="float")return TKind::Kw_float;
    if (t=="bool")return TKind::Kw_bool; if (t=="str")return TKind::Kw_str; if (t=="list")return TKind::Kw_list;
    return TKind::Id;
  }
};