Code from https://github.com/DreamSoule/ollvm17/issues/38

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
