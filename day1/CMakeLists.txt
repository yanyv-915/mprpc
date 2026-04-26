cmake_minimum_required(VERSION 3.30.5)
project(CacheServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 严格的环境变量获取
set(CONDA_DIR $ENV{CONDA_PREFIX})
if(NOT CONDA_DIR)
    message(FATAL_ERROR "未检测到 CONDA_PREFIX，请先激活 conda 环境！")
endif()

# 2. 提前定义路径变量（确保绝对路径）
set(CONDA_INC_DIR "${CONDA_DIR}/include")
set(CONDA_LIB_DIR "${CONDA_DIR}/lib")
set(LOCAL_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 调试信息：编译时请确认终端输出了正确的路径
message(STATUS ">>> Conda Include Path: ${CONDA_INC_DIR}")
message(STATUS ">>> Local Include Path: ${LOCAL_INC_DIR}")

# 3. 编译选项 (Debug + ASan)
add_compile_options(-O3 -fopenmp -Wall)
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# 4. 源文件扫描
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 5. 定义目标
add_executable(${PROJECT_NAME} ${SOURCES})

# 6. 【核心修复】精准头文件投送
# 注意：一定要确保 ${CONDA_INC_DIR} 下确实有 yaml-cpp 文件夹
target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE 
    ${LOCAL_INC_DIR}
    ${CONDA_INC_DIR}
)

# 7. 库查找与链接
find_library(FAISS_LIB NAMES faiss PATHS ${CONDA_LIB_DIR} NO_DEFAULT_PATH)
find_library(YAML_LIB NAMES yaml-cpp PATHS ${CONDA_LIB_DIR} NO_DEFAULT_PATH)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${YAML_LIB} 
    ${FAISS_LIB}
)

# 8. 运行时动态库路径设置
set_target_properties(${PROJECT_NAME} PROPERTIES 
    INSTALL_RPATH "${CONDA_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)