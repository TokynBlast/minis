#include "../include/driver.hpp"
#include "../include/context.hpp"
#include "../include/err.hpp"
#include "../include/io.hpp"

namespace minis {
  #include "compiler.cpp"
  #include "vm.cpp"
  #include "process.cpp"
}

#include <fstream>
#include <iterator>

namespace minsi {
  inline std::string ReadFile(const std::string& path){
    std::ifstream in(path, std::ios::binary);
    if(!in) MINIS_ERR("{T5}", *src, p.i, "cannot open "+path);
    return std::string(std::istreambuf_iterator<char>(in), {});
  }
  CompileToFile(const std::string& srcName,
                const std::string& srcText,
                const std::string& out) {
                  Source S{srcName, srcText};
                  ctx().src = &size_t s;

                  auto prep = minify(srcText);
                  ctx().posmap = std::move(prep.posmap);

                  Compiler C(S);

                  C.compileToFile
  }

  void run(const std::string& bcPath){
    VM vm;
    vm.load(bcPath);
    vm.run();
  }
}