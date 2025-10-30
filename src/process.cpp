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

// Preprocessing and minification
struct PreprocResult {
  std::string out;
  std::vector<size_t> posmap;   // out[i] -> raw offset
};

// NOTE: we keep logic same but when we copy any byte into `out`, we push raw index into posmap
static PreprocResult minify(const std::string& raw) {
  auto toks = lex_minis(raw);
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
      posmap.push_back(t.pos + std::min(k, t.text.size() ? t.text.size()-1 : 0));
    }

    prev = t;
  }
  return { std::move(out), std::move(posmap) };
}
