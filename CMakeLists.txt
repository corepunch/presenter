cmake_minimum_required(VERSION 3.10)
project(presenter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_library(tinyxml2_vendor STATIC
    third_party/tinyxml2.cpp
)

target_include_directories(tinyxml2_vendor PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party
)

add_library(tinyxml2::tinyxml2 ALIAS tinyxml2_vendor)

# ---------------------------------------------------------------------------
# presenter_core  —  static library used by both the SDL2 executable and the
# macOS SwiftUI application (via presenter_api).
# ---------------------------------------------------------------------------
add_library(presenter_core STATIC
    src/xml_parser.cpp
    src/style.cpp
    src/font.cpp
    src/renderer.cpp
    src/layout.cpp
    src/image.cpp
    src/highlight.cpp
    src/ui.cpp
    src/screenshot.cpp
    src/charts.cpp
)

target_include_directories(presenter_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(presenter_core PUBLIC SDL2::SDL2 tinyxml2::tinyxml2)

# ---------------------------------------------------------------------------
# presenter_api  —  C-API wrapper (links presenter_core; consumed by the app)
# ---------------------------------------------------------------------------
add_library(presenter_api STATIC
    src/presenter_api.cpp
)

target_include_directories(presenter_api PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(presenter_api PUBLIC presenter_core)

# ---------------------------------------------------------------------------
# presenter  —  standalone SDL2 executable (unchanged behaviour)
# ---------------------------------------------------------------------------
add_executable(presenter
    src/main.cpp
)

target_include_directories(presenter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(presenter PRIVATE presenter_core)

set_target_properties(presenter PROPERTIES
    OUTPUT_NAME presenter
)

add_executable(test_textbounds
    test/test_textbounds.cpp
    src/font.cpp
    src/renderer.cpp
    src/style.cpp
    src/layout.cpp
    src/image.cpp
    src/highlight.cpp
    src/ui.cpp
    src/charts.cpp
)

target_include_directories(test_textbounds PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(test_textbounds PRIVATE SDL2::SDL2 tinyxml2::tinyxml2)

set_target_properties(test_textbounds PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_layout
    test/test_layout.cpp
    src/layout.cpp
    src/style.cpp
)

target_include_directories(test_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(test_layout PRIVATE SDL2::SDL2 tinyxml2::tinyxml2)

set_target_properties(test_layout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_xml_parser
    test/test_xml_parser.cpp
    src/xml_parser.cpp
    src/style.cpp
)

target_include_directories(test_xml_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(test_xml_parser PRIVATE SDL2::SDL2 tinyxml2::tinyxml2)

set_target_properties(test_xml_parser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_image
    test/test_image.cpp
    src/image.cpp
)

target_include_directories(test_image PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

set_target_properties(test_image PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_highlight
    test/test_highlight.cpp
    src/highlight.cpp
)

target_include_directories(test_highlight PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

set_target_properties(test_highlight PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_screenshot
    test/test_screenshot.cpp
    src/screenshot.cpp
)

target_include_directories(test_screenshot PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(test_screenshot PRIVATE SDL2::SDL2)

set_target_properties(test_screenshot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
)

add_custom_target(copy_share ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/share
    ${CMAKE_BINARY_DIR}/share
)

add_custom_target(demo
    COMMAND $<TARGET_FILE:presenter> "${CMAKE_SOURCE_DIR}/demo/Nature Portfolio.slides"
    DEPENDS presenter
    USES_TERMINAL
)
