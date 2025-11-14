#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <string>
#include <print>

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
      std::print(stderr, "Error: cannot open file {}\n", path.c_str());
      return CString("");
    }
    fseek(f, 0, SEEK_END);
    size_t sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (buf) {
      if (fread(buf, 1, sz, f) != sz) {
        std::print(stderr, "Error reading file {}\n", path.c_str());
      }
      buf[sz] = '\0';
    }
    fclose(f);

    if (!buf) return CString("");

    CString result(buf);
    free(buf);
    return result;
  }

  void CompileToFile(const CString& srcName, const CString& srcText, const CString& out) {
    try {
      Source S{srcName, srcText};
      ctx().src = &S;

      auto tokens = tokenize(srcText.c_str(), srcText.size());

      CompileToFileImpl(tokens, out);

      std::print("Compilation successful: {}\n", out.c_str());

    } catch (const std::runtime_error& e) {
      std::print(stderr, "Compilation failed: {}\n", e.what());
      throw;
    }
  }

  void run(const CString& bcPath){
    VM vm;
    vm.load(bcPath);
    vm.run();
  }
}