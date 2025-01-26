cmake_minimum_required(VERSION 3.30)
project(Game_Networking)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src/Socket)
add_subdirectory(src/Server)
add_subdirectory(src/Client)