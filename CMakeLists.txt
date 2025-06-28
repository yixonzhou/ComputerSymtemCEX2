cmake_minimum_required(VERSION 3.22)
PROJECT(CEX2 LANGUAGES C)

set(CMAKE_C_STANDARD 23)

add_subdirectory(lib)

file(GLOB_RECURSE src_files src/*.c)
add_executable(f ${src_files})

target_include_directories(f PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(f clist)
