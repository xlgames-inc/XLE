#!/bin/bash
rm -r CMakeBuild/
mkdir CMakeBuild
cd CMakeBuild
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../Solutions/CMake/toolchain.cmake \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_RANLIB=/usr/bin/llvm-ranlib \
    -DCMAKE_AR=/usr/bin/llvm-ar \
    -DCMAKE_LINKER=/usr/bin/llvm-ld \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_SYSTEM_NAME=Linux \
    -G Ninja \
    ../Solutions/CMake/
