cmake_minimum_required(VERSION 3.16)
project(minis LANGUAGES CXX)

# --- compiler flags ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# --- user switches ---
# Default: build BOTH compiler and runner
option(RO "Runner Only" OFF)   # use: -DRO=ON
option(CO "Compiler Only" OFF) # use: -DCO=ON

set(BUILD_COMPILER ON)
set(BUILD_RUNNER   ON)
if(RO)
  set(BUILD_COMPILER OFF)
  set(BUILD_RUNNER   ON)
endif()
if(CO)
  set(BUILD_COMPILER ON)
  set(BUILD_RUNNER   OFF)
endif()

# --- include path ---
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- core sources shared by both ---
set(MINIS_CORE_SOURCES
  src/context.cpp
  src/diagnostics.cpp
  src/types.cpp
  src/value.cpp
  src/env.cpp
  src/io.cpp
  src/token.cpp
  src/lexer.cpp
  src/ast.cpp
  src/parser.cpp
  src/sema.cpp
  src/fold.cpp
  src/codegen.cpp
  src/bytecode.cpp
  src/builtins.cpp
  src/driver.cpp
)

# --- vm sources (runner) ---
set(MINIS_VM_SOURCES
  src/vm.cpp
)

# --- libraries ---
add_library(minis_core STATIC ${MINIS_CORE_SOURCES})
target_compile_options(minis_core PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_library(minis_vm STATIC ${MINIS_VM_SOURCES})
target_link_libraries(minis_vm PRIVATE minis_core)
target_compile_options(minis_vm PRIVATE -Wall -Wextra -Wno-unused-parameter)

# --- executables ---
if(BUILD_COMPILER)
  add_executable(cmin src/main_compiler.cpp)
  target_link_libraries(cmin PRIVATE minis_core)
endif()

if(BUILD_RUNNER)
  add_executable(mrun src/main_runner.cpp)
  target_link_libraries(mrun PRIVATE minis_core minis_vm)
endif()

# --- install (optional) ---
install(TARGETS cmin mrun
  RUNTIME DESTINATION bin
)

message(STATUS "Build compiler: ${BUILD_COMPILER}")
message(STATUS "Build runner  : ${BUILD_RUNNER}")