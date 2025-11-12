#include <iostream>
#include "include/driver.hpp"
#include "include/err.hpp"
#include "include/context.hpp"
#include "include/sso.hpp"

int main(int argc, char** argv){
  try{
    if(argc<2){
      std::print("Usage: mrun <program.mi>\n");
      return 2;
    }
    lang::run(lang::CString(argv[1]));
    return 0;
  }catch(const std::exception& e){
    std::print("{}\n", e.what());
    return 1;
  }
}