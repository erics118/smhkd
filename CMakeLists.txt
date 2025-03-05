cmake_minimum_required(VERSION 3.25)
project(smhkd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fstack-protector-strong)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
    add_definitions(-DNDEBUG)
endif()

find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

add_executable(${PROJECT_NAME}
    src/cli.cpp
    src/hotkey.cpp
    src/locale.cpp
    src/main.cpp
    src/parser.cpp
    src/service.cpp
    src/tokenizer.cpp
    src/utils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CARBON_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${CORE_FOUNDATION_FRAMEWORK}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE FALSE
)
