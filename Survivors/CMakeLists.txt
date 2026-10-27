cmake_minimum_required(VERSION 3.20)

project(Survivors)

# 类《吸血鬼幸存者》的俯视角生存游戏：只依赖 Engine，美术全部程序化生成
add_executable(${PROJECT_NAME}
    main.cpp
    SurvivorsGame.cpp
    SurvivorsArt.cpp
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
