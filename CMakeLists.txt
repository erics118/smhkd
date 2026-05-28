cmake_minimum_required(VERSION 3.28)
project(smhkd VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# shared between smhkd binary and test binary
add_library(smhkd_lib OBJECT
    src/cli/application.cpp
    src/cli/cli.cpp
    src/common/cf_string.cpp
    src/common/command.cpp
    src/common/config_path.cpp
    src/common/front_app.mm
    src/input/chord.cpp
    src/input/keysym.cpp
    src/input/locale.cpp
    src/input/modifier.cpp
    src/lang/config_loader.cpp
    src/lang/interpreter.cpp
    src/lang/parser.cpp
    src/lang/tokenizer.cpp
    src/runtime/hotkey_engine.cpp
    src/runtime/key_handler.cpp
    src/runtime/key_observer_handler.cpp
    src/runtime/process.cpp
    src/runtime/service.cpp
    src/common/string_util.cpp
)

target_include_directories(smhkd_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(smhkd_lib PUBLIC
    SMHKD_VERSION="${PROJECT_VERSION}"
)

# silence noisy diagnostics from Apple SDK headers under newer Clang
target_compile_options(smhkd_lib PUBLIC
    -Wno-elaborated-enum-base
    -Wno-deprecated-enum-enum-conversion
    -Wno-deprecated-declarations
    -Wno-deprecated-anon-enum-enum-conversion
    -Wdangling
    -Wdangling-gsl
    -Wreturn-stack-address
)

option(SMHKD_SANITIZERS "Enable Address/UB/implicit-conversion sanitizers and libc++ hardening" ON)

if(SMHKD_SANITIZERS)
    target_compile_options(smhkd_lib PUBLIC
        -fsanitize=address,undefined,implicit-conversion
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
        -g
    )
    target_link_options(smhkd_lib PUBLIC
        -fsanitize=address,undefined,implicit-conversion
    )
    target_compile_definitions(smhkd_lib PUBLIC
        _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG
    )
endif()

# macOS frameworks
find_library(CARBON_FRAMEWORK Carbon REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

target_link_libraries(smhkd_lib PUBLIC
    ${CARBON_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${CORE_FOUNDATION_FRAMEWORK}
)

add_executable(smhkd src/cli/main.cpp)
target_link_libraries(smhkd PRIVATE smhkd_lib)

# tests
enable_testing()
add_executable(smhkd_tests
    tests/main.cpp
    tests/test_tokenizer.cpp
    tests/test_parser.cpp
    tests/test_interpreter.cpp
)
target_link_libraries(smhkd_tests PRIVATE smhkd_lib)
target_include_directories(smhkd_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME smhkd_tests COMMAND smhkd_tests)
