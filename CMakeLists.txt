cmake_minimum_required(VERSION 3.28)
project(smhkd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure macOS SDK is visible to the dependency scanner
set(CMAKE_OSX_SYSROOT 
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CACHE PATH "macOS SDK root" FORCE)

# Ensure the macOS SDK is visible to the dependency scanner for modules
 

add_compile_options(-fstack-protector-strong)
add_compile_options(-Wno-elaborated-enum-base)
add_compile_options(-Wno-deprecated-declarations)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
    add_definitions(-DNDEBUG)
endif()

find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_sources(${PROJECT_NAME}
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            src/utils.ixx
            src/keysym.ixx
            src/modifier.ixx
            src/locale.ixx
            src/chord.ixx
            src/token.ixx
            src/log.ixx
            src/ast.ixx
            src/tokenizer.ixx
            src/parser.ixx
            src/interpreter.ixx
            src/ast_printer.ixx
            src/hotkey.ixx
            src/cli.ixx
            src/process.ixx
            src/service.ixx
            src/key_handler.ixx
            src/key_observer_handler.ixx
            
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CARBON_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${CORE_FOUNDATION_FRAMEWORK}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE SMHKD_USE_MODULES=1)

## Keep headers active for now while module co-exists

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE FALSE
)
