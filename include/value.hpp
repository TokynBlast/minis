inline bool operator==(const Value&a,const Value&b){
  if(a.t!=b.t) return false;
  switch(a.t){ case Type::Int:return std::get<long long>(a.v)==std::get<long long>(b.v);
    case Type::Float:return std::get<double>(a.v)==std::get<double>(b.v);
    case Type::Bool:return std::get<bool>(a.v)==std::get<bool>(b.v);
    case Type::Str:return std::get<std::string>(a.v)==std::get<std::string>(b.v);
    case Type::List:return std::get<std::vector<Value>>(a.v)==std::get<std::vector<Value>>(b.v);}
  return false;
}