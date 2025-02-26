cmake_minimum_required(VERSION 3.28.3)
project(falcon)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    set(FALCON_BACKEND src/falcon_windows.cpp)
else ()
    set(FALCON_BACKEND src/falcon_posix.cpp)
endif (WIN32)

add_library(falcon STATIC inc/falcon.h src/falcon_common.cpp inc/stream.h src/stream.cpp ${FALCON_BACKEND})
target_include_directories(falcon PUBLIC inc)
target_link_libraries(falcon PUBLIC spdlog::spdlog_header_only fmt::fmt-header-only)

if(WIN32)
    target_link_libraries(falcon PRIVATE Ws2_32)
else ()
    target_link_libraries(falcon PRIVATE pthread)
endif (WIN32)

add_subdirectory(externals)
add_subdirectory(samples)
add_subdirectory(tests)

