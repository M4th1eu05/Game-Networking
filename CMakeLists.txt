cmake_minimum_required(VERSION 3.30)
project(Game_Networking)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src/Socket)

# Create the server executable
add_subdirectory(src/Server)

# Create the client executable
add_subdirectory(src/Client)

# Platform-specific linking

if(WIN32)
    target_link_libraries(Socket ws2_32)
elseif(UNIX)
    target_link_libraries(Socket pthread)
endif()