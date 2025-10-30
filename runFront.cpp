#include <iostream>
#include <string>
#include "include/driver.hpp"
#include "include/err.hpp"
#include "include/context.hpp"

int main(int argc, char** argv){
  try{
    if(argc<2){
      std::print("Usage: mrun <program.mi>\n");
      return 2;
    }
    minis::RunMini(argv[1]);
    return 0;
  }catch(const std::exception& e){
    std::print("{}\n", e.what());
    return 1;
  }
}