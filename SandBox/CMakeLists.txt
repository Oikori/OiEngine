cmake_minimum_required(VERSION 3.20)

project(SandBox)

add_executable(${PROJECT_NAME} main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
