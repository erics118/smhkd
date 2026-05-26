cmake_minimum_required(VERSION 3.28)
project(smhkd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

add_executable(${PROJECT_NAME}
    src/cli/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SMHKD_VERSION="${PROJECT_VERSION}"
)

# silence noisy diagnostics from Apple SDK headers under newer Clang
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-elaborated-enum-base
    -Wno-deprecated-enum-enum-conversion
    -Wno-deprecated-declarations
    -Wno-deprecated-anon-enum-enum-conversion
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
