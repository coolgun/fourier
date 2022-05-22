cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

project(fourier CXX)
set(CMAKE_CXX_STANDARD 11)


set(FOURIER_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Location of 'fourier' directory")
set(NANOGUI_DIR "${FOURIER_DIR}/nanogui" CACHE PATH "Location of 'nanogui'")
add_subdirectory (${NANOGUI_DIR})

add_definitions(${NANOGUI_EXTRA_DEFS})

include_directories("${NANOGUI_DIR}/include")
include_directories(${NANOGUI_EXTRA_INCS})

add_executable(${PROJECT_NAME} main.cpp AngleEditor.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME} nanogui ${NANOGUI_EXTRA_LIBS})
