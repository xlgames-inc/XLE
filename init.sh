#!/bin/bash
rm -r CMakeBuild/
mkdir CMakeBuild
cd CMakeBuild
cmake -DCMAKE_TOOLCHAIN_FILE=../Solutions/CMake/toolchain_llvm.cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja ../Solutions/CMake/
