cmake_minimum_required(VERSION 3.10)
project(presenter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(presenter
    src/main.cpp
    src/parser.cpp
    src/font.cpp
    src/renderer.cpp
)

target_include_directories(presenter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(presenter PRIVATE SDL2::SDL2)

set_target_properties(presenter PROPERTIES
    OUTPUT_NAME presenter
)

add_executable(test_textbounds
    test/test_textbounds.cpp
    src/font.cpp
    src/renderer.cpp
)

target_include_directories(test_textbounds PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(test_textbounds PRIVATE SDL2::SDL2)

set_target_properties(test_textbounds PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
)
