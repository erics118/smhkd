build-type := "Debug"

default: debug

# Configure the build with specified build type
llvm-prefix := `brew --prefix llvm`

configure:
    cmake -S . -B ./build \
        -G Ninja \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
        -DCMAKE_BUILD_TYPE={{build-type}} \
        -DCMAKE_CXX_COMPILER={{llvm-prefix}}/bin/clang++ \
        -DSMHKD_SANITIZERS=ON;

# Build with current configuration
build: configure
    cmake --build ./build -j8;

# Build specifically in debug mode
debug:
    just build-type="Debug" build
    just sign

# Build specifically in release mode
release:
    just build-type="Release" build
    just sign

format:
    find src -name '*.cpp' -o -name '*.hpp' | xargs {{llvm-prefix}}/bin/clang-format -i

test: build
    ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
    LSAN_OPTIONS=suppressions={{justfile_directory()}}/.lsan-suppressions:print_suppressions=0 \
    ./build/smhkd_tests

clean:
    rm -rf ./build;

sign:
    codesign --force -i com.erics118.smhkd -s "smhkd-cert" ./build/smhkd
