# CMakeLists.txt

cmake_minimum_required(VERSION 4.5)

project(orbitals VERSION 0.1 LANGUAGES CXX)

find_package(Boost CONFIG REQUIRED COMPONENTS program_options)
find_package(PNG REQUIRED)

add_executable(orbitals
   src/main.cpp
)

target_compile_features(orbitals PRIVATE cxx_std_23)

target_link_libraries(orbitals PRIVATE
   Boost::program_options
   PNG::PNG
)

target_compile_definitions(orbitals PRIVATE
   ORBITALS_VERSION="${PROJECT_VERSION}"
)

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
      $<$<CONFIG:Release>:-Werror>
   )
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

