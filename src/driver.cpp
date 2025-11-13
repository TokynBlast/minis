#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <string>

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
    if(!in) throw std::invalid_argument(CString("cannot open ") + path);

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    return CString(content.c_str());
  }

  void CompileToFile(const CString& srcName,
                     const CString& srcText,
                     const CString& out) {
      Source S{srcName, srcText};
      ctx().src = &S;

      auto tokens = tokenize(srcText.c_str(), srcName.c_str());

      Compiler C(tokens);
      C.compileToFile(out);
  }

  void run(const CString& bcPath){
    VM vm;
    vm.load(bcPath);
    vm.run();
  }
}