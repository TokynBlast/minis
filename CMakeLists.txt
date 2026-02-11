cmake_minimum_required(VERSION 3.13)

# clang -S -std=gnu++23 -flto -O3 -march=native -mtune=native \
# src/vm.cpp \
# src/plugin.cpp \
# -o HEYYYY -I/fast_io/include

# ----------------------------------------
# Project setup
# ----------------------------------------
project(MinisMVME
    VERSION 0.1.0
)

# Optional: link Bcrypt on Windows
# target_link_libraries(MinisMVME PRIVATE Bcrypt)

# ----------------------------------------
# Language standards and flags
# ----------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


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
