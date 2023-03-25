cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

project(fourier CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC TRUE)

set(FOURIER_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Location of 'fourier' directory")
set(BLEND2D_DIR "${FOURIER_DIR}/blend2d" CACHE PATH "Location of 'blend2d'")
set(ASMJIT_DIR "${FOURIER_DIR}/AsmJit" CACHE PATH "Location of 'asmjit'")


set(BLEND2D_STATIC TRUE)
include("${BLEND2D_DIR}/CMakeLists.txt")

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

set(SRC_LIST main.cpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}  blend2d::blend2d)
target_link_libraries(${PROJECT_NAME}  Qt6::Widgets)

  
