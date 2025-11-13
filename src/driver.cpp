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

  CString ReadFile(const CString& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
      throw std::runtime_error("Could not open file: " + std::string(path.c_str()));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return CString(buffer.str().c_str());
  }

  void CompileToFile(const CString& input_path, const CString& source, const CString& output_path) {
    std::vector<Token> tokens = tokenize(source, input_path.c_str());

    Compiler compiler(tokens);
    compiler.compileToFile(output_path);
  }

  void run(const CString& bytecode_path) {
    VM vm;
    vm.load(bytecode_path);
    vm.run();
  }

  std::vector<Token> tokenize(const CString& source, const char* filename) {
    return tokenize(source.c_str(), filename);
  }
}