# Create the static library
add_library(Socket STATIC
        Socket.h
        Socket_windows.cpp
        Socket_posix.cpp
        Socket_common.cpp
)

# Détection de la plateforme
if(WIN32)
    target_sources(Socket PRIVATE Socket_windows.cpp)
else()
    target_sources(Socket PRIVATE Socket_posix.cpp)
endif()

target_include_directories(Socket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})