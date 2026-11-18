# CMakeLists.txt
cmake_minimum_required(VERSION 4.5.0)

project(orbitals VERSION 0.1 LANGUAGES CXX)

add_executable(orbitals src/main.cpp)
target_link_libraries(orbitals)
target_compile_features(orbitals PRIVATE cxx_std_23)

