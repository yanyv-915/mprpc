cmake_minimum_required(VERSION 3.30.5)

project(CacheServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O3 -fopenmp -Wall)

if(DEFINED ENV{CONDA_PREFIX})
    set(CONDA_DIR $ENV{CONDA_PREFIX})
    include_directories(${CONDA_DIR}/include)
    link_directories(${CONDA_DIR}/lib)
else()
    message((FATAL_ERROR "请先激活 faiss_lab 环境!"))
endif()


set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
)

target_link_libraries(
    ${PROJECT_NAME} faiss
)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "${CONDA_DIR}/lib")

target_link_directories(
    ${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/inlcude
)