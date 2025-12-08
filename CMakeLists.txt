cmake_minimum_required(VERSION 3.28)
project(smhkd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable(${PROJECT_NAME}
        src/cli/main.cpp
)

# c++ modules
target_sources(${PROJECT_NAME} PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        src/utils.ixx
        src/input/keysym.ixx
        src/input/modifier.ixx
        src/input/locale.ixx
        src/input/chord.ixx
        src/lang/token.ixx
        src/input/log.ixx
        src/lang/ast.ixx
        src/lang/tokenizer.ixx
        src/lang/parser.ixx
        src/lang/interpreter.ixx
        src/lang/ast_printer.ixx
        src/input/hotkey.ixx
        src/cli/cli.ixx
        src/runtime/process.ixx
        src/runtime/service.ixx
        src/runtime/key_handler.ixx
        src/runtime/key_observer_handler.ixx
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        SMHKD_VERSION="${PROJECT_VERSION}"
)

# macOS frameworks
find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CARBON_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK}
)
