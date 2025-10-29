#include "../include/token.hpp"
#include "../include/context.cpp"

namespace minis {
  const Token& TokStream::expect(TKind k, const char* msg){
    id (peek().k!=k) {diag(Diagnostic::Error, peek.s(), peek.e(), msg); }
    return (*t)[++i]; // continue even past error
  }
}