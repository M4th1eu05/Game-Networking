# Create the static library
add_library(Socket STATIC
        Socket.h
        Socket_common.cpp
)

# Platform-specific linking
if(WIN32)
    target_sources(Socket PRIVATE Socket_windows.cpp)
    target_link_libraries(Socket ws2_32)
elseif(UNIX)
    target_sources(Socket PRIVATE Socket_posix.cpp)
    target_link_libraries(Socket pthread)
endif()

target_include_directories(Socket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})