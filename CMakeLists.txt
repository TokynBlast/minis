cmake_minimum_required(VERSION 3.13)

#g++ -S -std=gnu++23 -flto -O3 -march=native -mtune=native src/vm.cpp src/plugin.cpp src/maths.f08 -lgfortran -fno-strict-aliasing -o HEYYYY -I/fast_io/include

# ----------------------------------------
# Project setup
# ----------------------------------------
project(MinisMVME
    VERSION 0.1.0
    LANGUAGES CXX Fortran
)

# Optional: link Bcrypt on Windows
# target_link_libraries(MinisMVME PRIVATE Bcrypt)

# ----------------------------------------
# Language standards and flags
# ----------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FVERSION "-std=f2008")
set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} ${FVERSION}")

# ----------------------------------------
# Main executable / library (adjust as needed)
# ----------------------------------------
# Example: main VM executable (Fortran + C++)
# add_executable(MinisMVME
#     src/main.f90
#     src/vm.cpp
# )

# target_link_libraries(MinisMVME PRIVATE
#     # Bcrypt
# )

# ----------------------------------------
# Plugins system
# Each subdirectory of plugins/ becomes a shared library
# named after the folder itself.
# ----------------------------------------
set(PLUGINS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/plugins")

# FIXME: Use -fotp-info-vec for Fortran
# This would prove the functions are properly vectorizing :)

if(EXISTS "${PLUGINS_DIR}")
    file(GLOB PLUGIN_DIRS RELATIVE "${PLUGINS_DIR}" "${PLUGINS_DIR}/*")

    foreach(PLUGIN_NAME IN LISTS PLUGIN_DIRS)
        if(IS_DIRECTORY "${PLUGINS_DIR}/${PLUGIN_NAME}")
            # Collect all source files in this plugin folder
            file(GLOB PLUGIN_SOURCES
                "${PLUGINS_DIR}/${PLUGIN_NAME}/*.f08"
                "${PLUGINS_DIR}/${PLUGIN_NAME}/*.c"
                "${PLUGINS_DIR}/${PLUGIN_NAME}/*.cpp"
                "${PLUGINS_DIR}/${PLUGIN_NAME}/*.cxx"
            )

            if(PLUGIN_SOURCES)
                add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

                # Optional: set output name explicitly (same as folder)
                set_target_properties(${PLUGIN_NAME} PROPERTIES
                    OUTPUT_NAME "${PLUGIN_NAME}"
                )

                # If plugins need to see common headers/modules:
                # target_include_directories(${PLUGIN_NAME} PRIVATE
                #     ${CMAKE_CURRENT_SOURCE_DIR}/include
                # )

                # If plugins need extra libs:
                # target_link_libraries(${PLUGIN_NAME} PRIVATE some_lib)
            endif()
        endif()
    endforeach()
endif()

# ----------------------------------------
# Future: Go, Ada, Rust (commented for now)
# ----------------------------------------

# ---- Go (via external build or custom command) ----
# find_program(GO_EXECUTABLE go)
# if(GO_EXECUTABLE)
#     # Example: build a Go plugin as a shared library
#     # add_custom_command(...)
# endif()

# ---- Ada (GNAT) ----
# enable_language(Ada OPTIONAL)
# if(CMAKE_Ada_COMPILER)
#     # add_library(ada_plugin SHARED plugins/ada_plugin/*.adb plugins/ada_plugin/*.ads)
# endif()

# ---- Rust (via Cargo or rustc) ----
# find_program(CARGO_EXECUTABLE cargo)
# if(CARGO_EXECUTABLE)
#     # add_custom_target(rust_plugin ALL
#     #     COMMAND ${CARGO_EXECUTABLE} build --release
#     #     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugins/rust_plugin
#     # )
# endif()
