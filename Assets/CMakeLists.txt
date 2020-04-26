set(Src
    ArchiveCache.cpp
    Assets.cpp
    AssetServices.cpp
    AssetSetManager.cpp
    AssetTraits.cpp
    AssetUtils.cpp
    AsyncMarkerGroup.cpp
    BlockSerializer.cpp
    ChunkFileContainer.cpp
    ChunkFile.cpp
    CompilationThread.cpp
    CompileAndAsyncManager.cpp
    ConfigFileContainer.cpp
    GeneralCompiler.cpp
    IntermediateAssets.cpp
    MainFileSystem.cpp
    MemoryFile.cpp
    MountingTree.cpp
    NascentChunk.cpp
    OSFileSystem.cpp
    RawFileAsset.cpp)

if (WIN32)
    list(APPEND WinAPISrc AsyncLoadOperation_WinAPI.cpp)
endif ()

BasicLibrary(Assets "${Src}")

target_link_libraries(Assets ForeignMisc)  # required to pull in interface include directories (etc)