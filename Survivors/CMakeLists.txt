cmake_minimum_required(VERSION 3.20)

project(Survivors)

# 类《吸血鬼幸存者》的俯视角生存游戏：只依赖 Engine，美术全部程序化生成。
# 游戏层按职责拆成多个 .cpp（都只定义 SurvivorsLayer 的成员函数，共享 SurvivorsGame.h）：
#   主循环/生命周期 · 敌人 · 武器 · 战斗与掉落 · 升级 · 世界渲染 · ImGui 面板
add_executable(${PROJECT_NAME}
    main.cpp
    SurvivorsGame.cpp
    SurvivorsEnemies.cpp
    SurvivorsWeapons.cpp
    SurvivorsCombat.cpp
    SurvivorsUpgrade.cpp
    SurvivorsRender.cpp
    SurvivorsUi.cpp
    SurvivorsArt.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# 着色器资源随可执行文件部署：运行时按当前工作目录读取 "assets/shader/..."
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying Survivors assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    VERBATIM
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
