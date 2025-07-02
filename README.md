# Build

```shell
git clone https://github.com/TIMER-err/ollvm-19
mkdir build
cd build
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_INSTALL_PREFIX=/opt/ollvm \
    ../ollvm-19/llvm
ninja -j6
ninja install
```

Also goes for windows

~~I hate msvc~~

# Credits
[OLLVM17](https://github.com/DreamSoule/ollvm17)

[OLLVM17#38](https://github.com/DreamSoule/ollvm17/issues/38)

[LLVM_PROJECT](https://github.com/llvm/llvm-project)
