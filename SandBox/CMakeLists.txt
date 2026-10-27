cmake_minimum_required(VERSION 3.20)

project(SandBox)

add_executable(${PROJECT_NAME}
    main.cpp
    PlaneGame.cpp
    SolarSystem.cpp
    ShadowDemo.cpp
    IndoorDemo.cpp
    VolumetricCloudDemo.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# 资源目录随可执行文件一起部署：业务代码按当前工作目录读取 "assets/..."（例如 assets/audio/laser.wav）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying SandBox assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    VERBATIM
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine
)
