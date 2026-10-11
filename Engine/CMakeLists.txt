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
# ImGui 符号由 Engine.dll 导出，使用方（如 SandBox）以 dllimport 直接调用 ImGui:: API
# 注意：不能用 target_compile_definitions —— CMake 会拆解值中的括号
target_compile_options(${PROJECT_NAME} INTERFACE
    "-DIMGUI_API=__declspec(dllimport)"
    "-DIMGUI_IMPL_API=__declspec(dllimport)"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor/imgui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    glfw
    spdlog
    stb_image
    imgui
)
