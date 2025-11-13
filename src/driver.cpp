#include <iostream>
#include <fstream>
#include <sstream>
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
  CString ReadFile(const CString& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
      std::cerr << "Error: cannot open file " << path.c_str() << std::endl;
      return CString("");
    }
    fseek(f, 0, SEEK_END);
    size_t sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (buf) {
      if (fread(buf, 1, sz, f) != sz) {
        std::cerr << "Error reading file " << path.c_str() << std::endl;
      }
      buf[sz] = '\0';
    }
    fclose(f);

    if (!buf) return CString("");

    CString result(buf);
    free(buf);
    return result;
  }

  void CompileToFile(const CString& srcName,
                     const CString& srcText,
                     const CString& out) {
      Source S{srcName, srcText};
      ctx().src = &S;

      auto tokens = tokenize(srcText, srcName.c_str());

      Compiler C(tokens);
      C.compileToFile(out);
  }

  void run(const CString& bcPath){
    VM vm;
    vm.load(bcPath);
    vm.run();
  }
}