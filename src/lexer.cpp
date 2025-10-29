struct Lexer {
  const std::string* src; size_t i=0, n=0; std::vector<Token> out;
  explicit Lexer(const std::string& s){ src=&s; n=s.size(); run(); }

  void ws(){ while(i<n && std::isspace((unsigned char)(*src)[i])) ++i; }

  static bool isIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
  static bool isIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

  void run(){
    while(true){
      ws(); size_t s=i; if(i>=n){ out.push_back({Kind::Eof,i,i,""}); break; }

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
        out.push_back({Kind::Str,s,i,src->substr(s,i-s)}); continue;
      }

      // numbers
      if (std::isdigit((unsigned char)(*src)[i]) ||
         (((*src)[i]=='+'||(*src)[i]=='-') && i+1<n && std::isdigit((unsigned char)(*src)[i+1]))){
        ++i; while(i<n && (std::isdigit((unsigned char)(*src)[i]) || (*src)[i]=='.')) ++i;
        out.push_back({Kind::Num,s,i,src->substr(s,i-s)}); continue;
      }

      // identifiers & keywords
      if (isIdStart((*src)[i])){
        ++i; while(i<n && isIdCont((*src)[i])) ++i;
        std::string t = src->substr(s,i-s);
        Kind k = keyword(t);
        out.push_back({k,s,i,std::move(t)}); continue;
      }

      // 2-char ops
      auto two = (i+1<n) ? (std::string()+(*src)[i]+(*src)[i+1]) : "";
      if (two=="&&"){ i+=2; out.push_back({Kind::And ,s,i,"&&"}); continue; }
      if (two=="||"){ i+=2; out.push_back({Kind::Or  ,s,i,"||"}); continue; }
      if (two=="=="){ i+=2; out.push_back({Kind::EQ  ,s,i,"=="}); continue; }
      if (two=="!="){ i+=2; out.push_back({Kind::NE  ,s,i,"!="}); continue; }
      if (two=="<="){ i+=2; out.push_back({Kind::LE  ,s,i,"<="}); continue; }
      if (two==">="){ i+=2; out.push_back({Kind::GE  ,s,i,">="}); continue; }

      // 1-char
      char c=(*src)[i++];
      Kind k = Kind::Eof;
      switch(c){
        case '(': k=Kind::LParen;   break;
        case ')': k=Kind::RParen;   break;
        case '{': k=Kind::LBrace;   break;
        case '}': k=Kind::RBrace;   break;
        case '[': k=Kind::LBracket; break;
        case ']': k=Kind::RBracket; break;
        case ',': k=Kind::Comma;    break;
        case ';': k=Kind::Semicolon;break;
        case ':': k=Kind::Colon;    break;
        case '=': k=Kind::Equal;    break;
        case '+': k=Kind::Plus;     break;
        case '-': k=Kind::Minus;    break;
        case '*': k=Kind::Star;     break;
        case '/': k=Kind::Slash;    break;
        case '!': k=Kind::Bang;     break;
        case '<': k=Kind::LT;       break;
        case '>': k=Kind::GT;       break;
        default:
          DIAG(Diagnostic::Warning, s,i, std::string("unknown char '")+c+"'");
          continue;
      }
      out.push_back({k,s,i,src->substr(s,i-s)});
    }
  }

  static Kind keyword(const std::string& t){
    if (t=="func")return Kind::Func;
    if (t=="let") return Kind::Let;
    if (t=="if")return Kind::If;
    if (t=="elif")return Kind::Elif;
    if (t=="else")return Kind::Else;
    if (t=="while")return Kind::While;
    if (t=="return")return Kind::Return; if (t=="break")return Kind::Break; if (t=="continue")return Kind::Continue;
    if (t=="del")return Kind::Del;
    if (t=="conv")return Kind::Conv;
    if (t=="exit")return Kind::Exit;
    if (t=="try")return Kind::Try;
    if (t=="except")return Kind::Except;
    if (t=="finally")return Kind::Finally;
    if (t=="lambda")return Kind::Lambda;
    if (t=="with")return Kind::With;
    if (t=="and")return Kind::And;
    if (t=="inline")return Kind::Inline;
    if (t=="tailcall")return Kind::Tail;
    if (t=="void")return Kind::Void;
    if (t=="true")return Kind::True;
    if (t=="false")return Kind::False;
    if (t=="null")return Kind::Null;
    if (t=="const")return Kind::Const;
    if (t=="static")return Kind::Static;
    if (t=="int")return Kind::Int;
    if (t=="float")return Kind::Float;
    if (t=="bool")return Kind::Bool;
    if (t=="str")return Kind::Str;
    if (t=="list")return Kind::List;
    return Kind::Id;
  }
};