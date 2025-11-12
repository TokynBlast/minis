#include <fstream>
#include <iterator>
#include <stdexcept>

#include "../include/driver.hpp"
#include "../include/context.hpp"
#include "../include/err.hpp"
#include "../include/io.hpp"
#include "../include/token.hpp"
#include "../include/lexer.hpp"
#include "../include/compiler.hpp"
#include "../include/process.hpp"
#include "../include/vm.hpp"
#include "../include/sso.hpp"

namespace lang {
  inline CString ReadFile(const CString& path){
    std::ifstream in(path.c_str(), std::ios::binary);
    if(!in) throw std::invalid_argument(CString("{T5} cannot open ") + path.c_str());
    return CString(std::string(std::istreambuf_iterator<char>(in), {}).c_str());
  }

  void CompileToFile(const CString& srcName,
                     const CString& srcText,
                     const CString& out) {
      Source S{srcName, srcText};
      ctx().src = &S;

      // produce tokens and attach filename so diagnostics can include the source name
      auto tokens = tokenize(srcText.c_str(), srcName.c_str());
      // Store tokens in context if needed
      ctx().tokens = std::move(tokens);

      Compiler C(S);
      C.compileToFile(out);
  }

  void run(const CString& bcPath){
    VM vm;
    vm.load(bcPath);
    vm.run();
  }
}