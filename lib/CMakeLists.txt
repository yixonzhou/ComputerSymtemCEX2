cmake_minimum_required(VERSION 3.22)

file(GLOB libs ${CMAKE_CURRENT_SOURCE_DIR}/*)

foreach (lib ${libs})
    if (IS_DIRECTORY ${lib} AND EXISTS ${lib}/CMakeLists.txt)
        add_subdirectory(${lib})
    endif ()
endforeach ()
