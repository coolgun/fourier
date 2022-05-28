cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

project(fourier CXX)
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(fltk)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/fltk fltk)

add_executable(fourier main.cpp)

target_link_libraries(fourier  fltk fltk_gl)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

