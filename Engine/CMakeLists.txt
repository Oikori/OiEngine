cmake_minimum_required(VERSION 3.20)

project(Engine)

file(GLOB_RECURSE SOURCE_FILE "./*.cpp")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    OI_SHARED_LIBRARY
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    OI_BUILDING_LIBRARY
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    glfw
    spdlog
    stb_image
)
