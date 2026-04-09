cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/quark-libs/raylib/include
    ${CMAKE_SOURCE_DIR}/quark-libs/rlImGui
    ${CMAKE_SOURCE_DIR}/src/headers
)

set(SOURCE_FILES
    src/main.cpp
    src/editor.cpp
    src/scene.cpp
    src/camera.cpp
    src/ImGuizmo.cpp
    src/tex.cpp
    src/models.cpp

    quark-libs/imgui/include/imgui.cpp
    quark-libs/imgui/include/imgui_draw.cpp
    quark-libs/imgui/include/imgui_widgets.cpp
    quark-libs/imgui/include/imgui_tables.cpp

    quark-libs/rlImGui/rlImGui.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to output directory"
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_SOURCE_DIR}/quark-libs/raylib/lib/raylib.lib
        winmm
        gdi32
        opengl32
    )

elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_SOURCE_DIR}/quark-libs/raylib/lib/libraylib.a
        m
        pthread
        dl
        rt
        X11
        GL
    )
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

elseif(APPLE)
    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_SOURCE_DIR}/quark-libs/raylib/lib/libraylib.a
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreVideo"
    )
endif()