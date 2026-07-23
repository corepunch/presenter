cmake_minimum_required(VERSION 3.10)
project(presenter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(tinyxml2 REQUIRED)

add_executable(presenter
    src/main.cpp
    src/xml_parser.cpp
    src/style.cpp
    src/font.cpp
    src/renderer.cpp
    src/layout.cpp
    src/image.cpp
    src/highlight.cpp
)

target_include_directories(presenter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(presenter PRIVATE SDL2::SDL2 tinyxml2::tinyxml2)

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

target_link_libraries(test_layout PRIVATE tinyxml2::tinyxml2)

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

target_link_libraries(test_xml_parser PRIVATE tinyxml2::tinyxml2)

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
    COMMAND $<TARGET_FILE:presenter> ${CMAKE_SOURCE_DIR}/demo/demo.xml
    DEPENDS presenter
    USES_TERMINAL
)
