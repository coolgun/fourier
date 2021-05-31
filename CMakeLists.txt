cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

project(fourier CXX)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(wxWidgets)

set(SRC_LIST main.cpp wxAngleEditor.cpp)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} wx::net wx::core wx::base)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
  
