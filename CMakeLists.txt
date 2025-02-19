cmake_minimum_required(VERSION 3.5)

project(LEXT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(__linux__)

link_libraries(
   # -lrt
)


add_executable(LEXT
    src/main.cpp
    src/lext.h
    src/lext.cpp
)

install(TARGETS LEXT
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#g++ writer.cpp -o writer -lrt
