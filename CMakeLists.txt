cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/quark-libs/raylib/include
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
    quark-libs/imgui/include/rlImGui.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

set(RAYLIB_LIB_DIR "${CMAKE_SOURCE_DIR}/quark-libs/raylib/lib")

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ASSETS_DIR}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ASSETS_DIR}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to output directory"
    )
else()
    message(WARNING "Assets directory not found: ${ASSETS_DIR}. Skipping asset copy step.")
endif()

if(WIN32)
    target_link_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR})
    target_link_libraries(${PROJECT_NAME}
        raylib
        winmm
        gdi32
        opengl32
    )
elseif(UNIX AND NOT APPLE)
    if(EXISTS "${RAYLIB_LIB_DIR}/libraylib.a")
        set(RAYLIB_LIB "${RAYLIB_LIB_DIR}/libraylib.a")
    elseif(EXISTS "${RAYLIB_LIB_DIR}/libraylib.so.5.5.0")
        set(RAYLIB_LIB "${RAYLIB_LIB_DIR}/libraylib.so.5.5.0")
    else()
        set(RAYLIB_LIB raylib)
    endif()

    target_link_libraries(${PROJECT_NAME}
        ${RAYLIB_LIB}
        GL
        m
        pthread
        dl
        rt
        X11
    )
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif(APPLE)
    target_link_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_DIR})
    target_link_libraries(${PROJECT_NAME}
        raylib
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreVideo"
    )
endif()
