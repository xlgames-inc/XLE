rmdir /S /Q CMakeBuild
mkdir CMakeBuild
cd CMakeBuild
"C:\Program Files\CMake\bin\cmake.exe" ^
    -DCMAKE_TOOLCHAIN_FILE=../Solutions/CMake/toolchain.cmake ^
    %* ^
    -G "Visual Studio 15 2017 Win64" ^
    ../Solutions/Global/
cd ..
