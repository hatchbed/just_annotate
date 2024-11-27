cmake_minimum_required(VERSION 3.16)
project(just_annotate LANGUAGES CXX)

set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(PNG REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_GL IMPORTED_TARGET REQUIRED gstreamer-gl-1.0)
pkg_check_modules(LIBCRYPTO IMPORTED_TARGET REQUIRED libcrypto)
pkg_check_modules(LIBSSL IMPORTED_TARGET REQUIRED libssl)
pkg_check_modules(NLOHMANN_JSON IMPORTED_TARGET REQUIRED REQUIRED nlohmann_json)
pkg_check_modules(SPDLOG IMPORTED_TARGET REQUIRED REQUIRED spdlog)

set(HELLOIMGUI_DOWNLOAD_FREETYPE_IF_NEEDED ON)

include(${PROJECT_SOURCE_DIR}/cmake/CMakeRC.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/hello_imgui.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/imgui-filebrowser.cmake)

##########################################################
# Build
##########################################################

cmrc_add_resource_library(${PROJECT_NAME}-rc
    ALIAS ${PROJECT_NAME}::rc
    NAMESPACE ${PROJECT_NAME}::rc 
    WHENCE ${hello_imgui_SOURCE_DIR}/hello_imgui_assets/fonts/
        ${hello_imgui_SOURCE_DIR}/hello_imgui_assets/fonts/DroidSans.ttf
        ${hello_imgui_SOURCE_DIR}/hello_imgui_assets/fonts/fontawesome-webfont.ttf)

add_executable(${PROJECT_NAME}
    src/annotation_class_dialog.cpp
    src/annotation_store.cpp
    src/config_store.cpp
    src/hash.cpp
    src/imgui_util.cpp
    src/main.cpp
    src/video_file.cpp
    ${hello_imgui_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${hello_imgui_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl2.cpp)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
set_bundle_variables_defaults(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE 
    include
    ${imgui-filebrowser_SOURCE_DIR}
    ${tomlplusplus_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    hello_imgui
    OpenGL::GL
    OpenGL::GLU
    PkgConfig::GSTREAMER
    PkgConfig::GSTREAMER_APP
    PkgConfig::GSTREAMER_GL
    PkgConfig::LIBCRYPTO
    PkgConfig::LIBSSL
    PkgConfig::NLOHMANN_JSON
    PkgConfig::SPDLOG
    PNG::PNG
    ${PROJECT_NAME}::rc)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

install(TARGETS ${PROJECT_NAME})