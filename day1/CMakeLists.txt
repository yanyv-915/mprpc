cmake_minimum_required(VERSION 3.30.5)

project(CacheServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")


add_executable(
    CacheServer
    src/main.cpp
    src/LRU.cpp
    src/TcpServer.cpp
    src/ThreadPool.cpp
)

target_link_directories(
    CacheServer PRIVATE 
    ${CMAKE_SOURCE_DIR}/inlcude
)