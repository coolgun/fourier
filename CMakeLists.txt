cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

project(fourier CXX)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC TRUE)

set(FOURIER_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Location of 'fourier' directory")

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

set(SRC_LIST main.cpp)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}  Qt6::Widgets)
