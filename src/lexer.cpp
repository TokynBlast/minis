#include <cctype>
#include "../include/lexer.hpp"


namespace minis {
  static bool isIdStart(char c){ return std::isalpha((unsigned char)c)||c=='_'; }
  static bool isIdCont (char c){ return std::isalnum((unsigned char)c)||c=='_'||c=='.'; }

  void Lexer::ws(){ while(i<n && std::isspace((unsigned char)(*src)[i])) ++i; }

  void Lexer::run(){
    while(true){
      ws(); size_t s=i; if(i>=n){ out.push_back({Tok::Eof,i,i,""}); break; }

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
        out.push_back({Tok::Str,s,i,src->substr(s,i-s)}); continue;
      }

      // numbers
      if (std::isdigit((unsigned char)(*src)[i]) ||
        (((*src)[i]=='+'||(*src)[i]=='-') && i+1<n && std::isdigit((unsigned char)(*src)[i+1]))){
        ++i; while(i<n && (std::isdigit((unsigned char)(*src)[i]) || (*src)[i]=='.')) ++i;
        out.push_back({Tok::Num,s,i,src->substr(s,i-s)}); continue;
      }

      // identifiers & keywords
      if (isIdStart((*src)[i])){
        ++i; while(i<n && isIdCont((*src)[i])) ++i;
        std::string t = src->substr(s,i-s);
        Tok k = keyword(t);
        out.push_back({k,s,i,std::move(t)}); continue;
      }

      // 2-char ops
      auto two = (i+1<n) ? (std::string()+(*src)[i]+(*src)[i+1]) : "";
      if (two=="&&"){ i+=2; out.push_back({Tok::And ,s,i,"&&"}); continue; }
      if (two=="||"){ i+=2; out.push_back({Tok::Or  ,s,i,"||"}); continue; }
      if (two=="=="){ i+=2; out.push_back({Tok::EQ  ,s,i,"=="}); continue; }
      if (two=="!="){ i+=2; out.push_back({Tok::NE  ,s,i,"!="}); continue; }
      if (two=="<="){ i+=2; out.push_back({Tok::LE  ,s,i,"<="}); continue; }
      if (two==">="){ i+=2; out.push_back({Tok::GE  ,s,i,">="}); continue; }

      // 1-char
      char c=(*src)[i++];
      Tok k = Tok::Eof;
      switch(c){
        case '(': k=Tok::LParen;   break;
        case ')': k=Tok::RParen;   break;
        case '{': k=Tok::LBrace;   break;
        case '}': k=Tok::RBrace;   break;
        case '[': k=Tok::LBracket; break;
        case ']': k=Tok::RBracket; break;
        case ',': k=Tok::Comma;    break;
        case ';': k=Tok::Semicolon;break;
        case ':': k=Tok::Colon;    break;
        case '=': k=Tok::Equal;    break;
        case '+': k=Tok::Plus;     break;
        case '-': k=Tok::Minus;    break;
        case '*': k=Tok::Star;     break;
        case '/': k=Tok::FSlash;   break;
        case '\\':k=Tok::BSlash;   break;
        case '!': k=Tok::Bang;     break;
        case '<': k=Tok::LT;       break;
        case '>': k=Tok::GT;       break;
        case '$': k=Tok::Dollar;   break;
        case '_': k=Tok::UScore;   break;
        case '&': k=Tok::Amp;      break;
        case '^': k=Tok::Karet;    break;
        case '%': k=Tok::Percent;  break;
        case '.': k=Tok::Dot;      break;
        case '\'':k=Tok::SQuote;   break;
        case '"': k=Tok::DQuote;   break;
        case '|': k=Tok::Pipe;     break;
        default:
          DIAG(DiagKind::Warning, s,i, std::string("unknown char '")+c+"'");
          continue;
      }
      out.push_back({k,s,i,src->substr(s,i-s)});
    }
  }

  Tok Lexer::keyword(const std::string& t){
    if (t=="func")return Tok::Func;
    if (t=="let") return Tok::Let;
    if (t=="if")return Tok::If;
    if (t=="elif")return Tok::Elif;
    if (t=="else")return Tok::Else;
    if (t=="while")return Tok::While;
    if (t=="return")return Tok::Return;
    if (t=="break")return Tok::Break;
    if (t=="continue")return Tok::Cont;
    if (t=="del")return Tok::Del;
    if (t=="conv")return Tok::Conv;
    if (t=="exit")return Tok::Exit;
    if (t=="try")return Tok::Try;
    if (t=="except")return Tok::Except;
    if (t=="finally")return Tok::Finally;
    if (t=="lambda")return Tok::Lambda;
    if (t=="with")return Tok::With;
    if (t=="and")return Tok::And;
    if (t=="inline")return Tok::Inline;
    if (t=="tail")return Tok::Tail;
    if (t=="void")return Tok::Void;
    if (t=="true")return Tok::True;
    if (t=="false")return Tok::False;
    if (t=="null")return Tok::Null;
    if (t=="const")return Tok::Const;
    if (t=="static")return Tok::Static;
    if (t=="int")return Tok::Int;
    if (t=="float")return Tok::Float;
    if (t=="bool")return Tok::Bool;
    if (t=="str")return Tok::Str;
    if (t=="list")return Tok::List;
    return Tok::Id;
  }
}