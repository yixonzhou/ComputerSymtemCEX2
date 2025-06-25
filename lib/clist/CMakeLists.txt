cmake_minimum_required(VERSION 3.22)
project(CEX2)

add_library(clist STATIC ${CMAKE_CURRENT_SOURCE_DIR}/clist.c)

target_include_directories(clist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(clist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
