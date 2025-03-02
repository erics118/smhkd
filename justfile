configure: 
    cmake -DCMAKE_BUILD_TYPE:STRING=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
        -DCMAKE_CXX_COMPILER:FILEPATH=/opt/homebrew/opt/llvm/bin/clang++ \
        -B ./build \
        -G Ninja

build: configure
    cmake --build ./build -j8;

clean:
    rm -rf ./build;
