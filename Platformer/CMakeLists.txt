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

# 背景音乐 / 音效随可执行文件部署：运行时按当前工作目录读取 "assets/audio/..."
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying Platformer assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    VERBATIM
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
