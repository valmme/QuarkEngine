cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_ENGINE "Build Quark Engine" ON)
option(BUILD_PLUGIN "Build Plugin" OFF)

set(QC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS    ON CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/include
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src
)

add_subdirectory(quark-libs/QuarkCore)

add_library(imgui SHARED
    quark-libs/imgui/include/imgui.cpp
    quark-libs/imgui/include/imgui_draw.cpp
    quark-libs/imgui/include/imgui_widgets.cpp
    quark-libs/imgui/include/imgui_tables.cpp
    imgui/imgui_impl_sdl3.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_impl_vulkan.cpp
    src/qcImGui.cpp
    src/ImGuizmo.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/quark-libs/freetype/include
)

target_link_libraries(imgui PUBLIC
    SDL3::SDL3
    QuarkCore-OpenGL
    QuarkCore-Vulkan
    Vulkan::Vulkan
)

if(WIN32)
    target_compile_definitions(imgui
        PRIVATE BUILD_LIBTYPE_SHARED
        PRIVATE IMGUI_API=__declspec\(dllexport\)
    )
else()
    target_compile_definitions(imgui
        PRIVATE BUILD_LIBTYPE_SHARED
        PRIVATE "IMGUI_API=__attribute__((visibility(\"default\")))"
    )
endif()

set_target_properties(imgui PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

if(APPLE)
    set_target_properties(imgui PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(imgui PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

if(BUILD_ENGINE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    set(EDITOR_FILES
        src/editor/editor_assets.cpp
        src/editor/editor_components_ui.cpp
        src/editor/editor_entity.cpp
        src/editor/editor_ui.cpp
        src/editor/editor_utils.cpp
        src/editor/editor_viewers.cpp
        src/editor/editor.cpp
        src/editor/editor_hierarchy_utils.cpp
        src/editor/editor_preferences.cpp
        src/editor/editor_file_utils.cpp
    )

    set(SOURCE_FILES
        src/application.cpp
        src/camera.cpp
        src/component.cpp
        src/command_line.cpp
        src/editable_mesh.cpp
        src/entity.cpp
        src/hub.cpp
        src/language_manager.cpp
        src/lighting.cpp
        src/main.cpp
        src/models.cpp
        src/project.cpp
        src/plugins/plugin_manager.cpp
        src/scene.cpp
        src/tex.cpp
        src/text_mesh.cpp
    )

    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${EDITOR_FILES})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    )

    if(WIN32)
        target_sources(${PROJECT_NAME} PRIVATE QuarkEngine.rc)
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/quark-libs/freetype/include
        ${CMAKE_SOURCE_DIR}/include
    )

    if(WIN32)
        target_compile_definitions(${PROJECT_NAME}
            PRIVATE USE_LIBTYPE_SHARED
            PRIVATE IMGUI_API=__declspec\(dllimport\)
        )
    else()
        target_compile_definitions(${PROJECT_NAME}
            PRIVATE USE_LIBTYPE_SHARED
            PRIVATE "IMGUI_API=__attribute__((visibility(\"default\")))"
        )
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        QuarkCore-OpenGL
        QuarkCore-Vulkan
        imgui
    )

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 opengl32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE GL m pthread dl rt X11)
    elseif(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreAudio"
            "-framework CoreVideo"
        )
    endif()

    if(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )
    elseif(UNIX)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )

    if(WIN32)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore>
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:imgui>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:ZLIB::ZLIB>
                $<TARGET_FILE:freetype>
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/debug/OpenAL32.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/release/OpenAL32.dll>
                "${VULKAN_RUNTIME_DLL}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools-optd.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SPIRV-Tools-opt.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Toolsd.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SPIRV-Tools.dll"
        )
    endif()

    if(UNIX)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore>
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:imgui>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:freetype>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying shared libraries next to ${PROJECT_NAME} (Linux/macOS)..."
        )
        if(APPLE)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:assimp::assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:freetype> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>
                COMMENT "Creating macOS shared library compatibility symlinks..."
            )
        else()
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:freetype> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.so.6,libfreetype.so.6>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:assimp::assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.so.6,libassimp.so.6>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:SDL3::SDL3> $<TARGET_FILE_DIR:${PROJECT_NAME}>/libSDL3.so.0
                COMMENT "Creating shared library symlinks next to ${PROJECT_NAME} (Linux)..."
            )
        endif()
    endif()
endif()

if(BUILD_PLUGIN)
    add_library(MyPlugin SHARED
        plugins/MyPlugin/plugin.cpp
    )

    target_include_directories(MyPlugin PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins
    )

    if(WIN32)
        target_link_options(MyPlugin PRIVATE -Wl,--allow-shlib-undefined)
    endif()

    set_target_properties(MyPlugin PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
    )

    if(UNIX)
        set_target_properties(MyPlugin PROPERTIES
            BUILD_RPATH "$ORIGIN/.."
            INSTALL_RPATH "$ORIGIN/.."
        )
    endif()
endif()