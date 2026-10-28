cmake_minimum_required(VERSION 3.20)

project(OiEngine VERSION 1.0 LANGUAGES CXX)
# miniaudio 以 C 源码编译（vendor/miniaudio/miniaudio.c）
enable_language(C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_host_system_information(RESULT CPU_NUMS QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "CPU NUMS on current server is: ${CPU_NUMS}")
math(EXPR JOBS "${CPU_NUMS} + ${CPU_NUMS} / 2")
set(CMAKE_BUILD_PARALLEL_LEVEL ${JOBS} CACHE STRING "并行编译线程数")
message(STATUS "Compiler default work jobs: ${CMAKE_BUILD_PARALLEL_LEVEL}")

add_subdirectory(vendor)
add_subdirectory(Engine)
add_subdirectory(SandBox)
add_subdirectory(Platformer)
add_subdirectory(Survivors)
