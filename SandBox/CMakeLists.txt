cmake_minimum_required(VERSION 3.20)

project(SandBox)

add_executable(${PROJECT_NAME}
    main.cpp
    PlaneGame.cpp
    SolarSystem.cpp
    ShadowDemo.cpp
    IndoorDemo.cpp
    VolumetricCloudDemo.cpp
    Platformer/Platformer.cpp
)

# 子目录（如 Platformer/）里的源文件也需要能找到 SandBox 根目录下的公共头（RenderHelpers.h 等）
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
