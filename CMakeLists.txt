cmake_minimum_required(VERSION 3.28)
project(smhkd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE AND NOT DEFINED CMAKE_OSX_SYSROOT)
    # Discover macOS SDK path without hardcoding
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(SDK_PATH)
        set(CMAKE_OSX_SYSROOT "${SDK_PATH}" CACHE PATH "macOS SDK root" FORCE)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_SMHKD_RELEASE_OPTS -O3)
endif()

find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

add_executable(${PROJECT_NAME}
        src/cli/main.cpp
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -fstack-protector-strong
    -Wno-elaborated-enum-base
    -Wno-deprecated-declarations
    ${_SMHKD_RELEASE_OPTS}
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
endif()

target_sources(${PROJECT_NAME}
    PRIVATE
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
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CARBON_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${CORE_FOUNDATION_FRAMEWORK}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SMHKD_USE_MODULES=1
    SMHKD_VERSION="${PROJECT_VERSION}"
)

## Keep headers active for now while module co-exists

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE FALSE
)
