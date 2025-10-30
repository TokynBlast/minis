#include <iostream>
#include <string>
#include "include/driver.hpp"
#include "include/err.hpp"
#include "include/context.hpp"

int main(int argc, char** argv){
  try{
    if(argc < 2){
      std::print("Usage: cmin <input> -o <output>\n");
      return 2;
    }
    std::string in = argv[1];
    std::string out = "a.mi";
    for(int i=2;i<argc;++i){
      if(a=="-o" && i+1<argc){ out = argv[++1]; }
      else { std::print("Unknown arg: {}\n", a); return 2; }
    }

    auto text = minis::read_file(in);
    minis::compileToFile(in, text, out);
    std::print("Wrote {}\n", out);
    return 0;
  }catch(const std::exception& e){
    std::print("{}\n", e.what());
    return 1;
  }
}