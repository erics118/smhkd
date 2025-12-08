build-type := "Debug"

default: debug

# Configure the build with specified build type
configure: 
    cmake -S . -B ./build \
        -G Ninja \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
        -DCMAKE_BUILD_TYPE={{build-type}};

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

clean:
    rm -rf ./build;

sign:
	codesign --force -s - ./build/smhkd
