cmake_minimum_required(VERSION 3.20)

project(Platformer)

# 独立平台跳跃小游戏：只依赖 Engine，几何与关卡全部程序化生成
add_executable(${PROJECT_NAME}
    main.cpp
    Platformer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
