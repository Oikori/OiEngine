cmake_minimum_required(VERSION 3.20)

project(vendor)

add_subdirectory(glad)
add_subdirectory(glfw)
add_subdirectory(spdlog)
add_subdirectory(stb_image)

# Dear ImGui：源码位于 submodule（vendor/imgui，上游不提供 CMakeLists），
# 构建定义保留在主仓库（submodule 目录内的文件不受版本控制）
# 编译为 OBJECT 库整体并入 Engine.dll：IMGUI_API/IMGUI_IMPL_API 定义为 dllexport，
# SandBox 侧由 Engine 的 INTERFACE 选项获得 dllimport，可直接调用 ImGui:: API
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

add_library(imgui OBJECT
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp          # ShowDemoWindow 等示例代码（学习/调试参考，可选）
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# imgui_impl_glfw 需要 GLFW 头文件
# opengl3 backend 使用自带 loader（imgui_impl_opengl3_loader.h），不依赖 glad
target_link_libraries(imgui PUBLIC
    glfw
)

# 注意：不能用 target_compile_definitions —— CMake 会拆解值中的括号（生成畸形的 -D），
# 这里以编译器选项形式原样传递
target_compile_options(imgui PRIVATE
    "-DIMGUI_API=__declspec(dllexport)"
)
