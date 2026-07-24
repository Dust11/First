cmake_minimum_required(VERSION 3.21)

project(MingCKeyOverlay
    VERSION 0.1.0
    DESCRIPTION "鸣潮弹幕式按键提示 overlay"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 静态运行时 + UTF-8
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/utf-8 /W4 /WX-)
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

# 统一 exe 输出目录，方便 post-build 拷贝资源
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# nlohmann/json 单头文件
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ImGui docking branch
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

set(SOURCES
    src/main.cpp
    src/utils/Logger.cpp
    src/utils/TextEncoding.cpp
    src/utils/ResourceLoader.cpp
    src/utils/HotkeyManager.cpp
    src/utils/FileWatcher.cpp
    src/core/TeamRotation.cpp
    src/core/ConfigManager.cpp
    src/core/PlaybackEngine.cpp
    src/core/KeyDetector.cpp
    src/overlay/Direct2DRenderer.cpp
    src/overlay/OverlayWindow.cpp
    src/overlay/VisualRenderer.cpp
    src/editor/EditorWindow.cpp
    src/editor/EditorComponents.cpp
)

set(HEADERS
    src/utils/Logger.h
    src/utils/TextEncoding.h
    src/utils/ResourceLoader.h
    src/utils/HotkeyManager.h
    src/utils/FileWatcher.h
    src/core/TeamRotation.h
    src/core/ConfigManager.h
    src/core/PlaybackEngine.h
    src/core/KeyDetector.h
    src/overlay/IRenderer.h
    src/overlay/Direct2DRenderer.h
    src/overlay/OverlayWindow.h
    src/overlay/VisualRenderer.h
    src/editor/EditorWindow.h
    src/editor/EditorComponents.h
)

add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${IMGUI_SOURCES}
    app.manifest
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    d2d1
    dwrite
    dcomp
    dxgi
    d3d11
    ole32
    uuid
    shell32
    shlwapi
    winmm
    imm32
    version
    comctl32
)

# 复制 profiles/ 与 assets/ 到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/profiles
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/profiles
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    COMMENT "Copying profiles/ and assets/ to output directory"
)

# install 绿色包
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY profiles/ DESTINATION profiles)
install(DIRECTORY assets/ DESTINATION assets)
install(FILES README.md DESTINATION .)

# CPack ZIP 绿色包配置
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "MingCKeyOverlay")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "MingC")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "鸣潮弹幕式按键提示 overlay")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_ARCHIVE_FILE_NAME "MingCKeyOverlay-${PROJECT_VERSION}-win64")
include(CPack)
