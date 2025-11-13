cmake_minimum_required(VERSION 3.16)
project(minis LANGUAGES CXX)

# --- compiler flags ---
set(CMAKE_CXX_STANDARD 23)
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
  src/lexer.cpp
  src/driver.cpp
  src/compiler.cpp
  src/process.cpp
  src/ugly.cpp
  src/config.cpp
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
  add_executable(cmin compileFront.cpp)
  target_link_libraries(cmin PRIVATE minis_core)
  target_compile_options(cmin PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(BUILD_RUNNER)
  add_executable(mrun runFront.cpp)
  target_link_libraries(mrun PRIVATE minis_core minis_vm)
  target_compile_options(mrun PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# --- conditional install ---
if(BUILD_COMPILER AND BUILD_RUNNER)
  install(TARGETS cmin mrun RUNTIME DESTINATION bin)
elseif(BUILD_COMPILER)
  install(TARGETS cmin RUNTIME DESTINATION bin)
elseif(BUILD_RUNNER)
  install(TARGETS mrun RUNTIME DESTINATION bin)
endif()

message(STATUS "Build compiler: ${BUILD_COMPILER}")
message(STATUS "Build runner  : ${BUILD_RUNNER}")