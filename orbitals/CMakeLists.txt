# CMakeLists.txt
cmake_minimum_required(VERSION 4.5)

project(orbitals VERSION 0.1 LANGUAGES CXX)

add_executable(orbitals
   src/main.cpp
)

target_compile_features(orbitals PRIVATE cxx_std_23)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
   target_compile_options(orbitals PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
   )
endif()

# Treat warnings as errors in Release builds.
set_target_properties(orbitals PROPERTIES
   COMPILE_WARNING_AS_ERROR "$<CONFIG:Release>"
)

