set(Src
    BasicSceneParser.cpp
    CameraManager.cpp
    DebugHotKeys.cpp
    FrameRig.cpp
    InitDebugDisplays.cpp
    InputListener.cpp
    MainInputHandler.cpp
    OverlaySystem.cpp
    PlatformRigUtil.cpp
    Screenshot.cpp
    UnitCamera.cpp)
set(Headers 
    AllocationProfiler.h
    BasicSceneParser.h
    CameraManager.h
    DebugHotKeys.h
    FrameRig.h
    InputListener.h
    InputTranslator.h
    MainInputHandler.h
    OverlappedWindow.h
    OverlaySystem.h
    PlatformRigUtil.h
    Screenshot.h
    UnitCamera.h)

set(DebuggingDisplaysSrc 
    DebuggingDisplays/BufferUploadDisplay.cpp
    DebuggingDisplays/ConsoleDisplay.cpp
    DebuggingDisplays/CPUProfileDisplay.cpp
    DebuggingDisplays/DynamicImpostersDisplay.cpp
    DebuggingDisplays/GPUProfileDisplay.cpp
    DebuggingDisplays/HierarchicalSpikesDisplay.cpp
    DebuggingDisplays/HistoricalProfilerDisplay.cpp
    DebuggingDisplays/PlacementsDisplay.cpp
    DebuggingDisplays/TestDisplays.cpp)
set(DebuggingDisplaysHeaders 
    DebuggingDisplays/BufferUploadDisplay.h
    DebuggingDisplays/ConsoleDisplay.h
    DebuggingDisplays/CPUProfileDisplay.h
    DebuggingDisplays/DynamicImpostersDisplay.h
    DebuggingDisplays/GPUProfileDisplay.h
    DebuggingDisplays/HierarchicalSpikesDisplay.h
    DebuggingDisplays/HistoricalProfilerDisplay.h
    DebuggingDisplays/PlacementsDisplay.h
    DebuggingDisplays/TestDisplays.h)

if (WIN32)
    set(WinSrc WinAPI/AllocationProfiler.cpp WinAPI/InputTranslator.cpp WinAPI/OverlappedWindow.cpp WinAPI/RunLoop_WinAPI.cpp)
    set(WinHeaders RunLoop_WinAPI.h)
endif()

BasicLibrary(PlatformRig "${Src};${WinSrc};${DebuggingDisplaysSrc}" "${Headers};${WinHeaders};${DebuggingDisplaysHeaders}")

target_link_libraries(PlatformRig ForeignMisc)  # required to pull in interface include directories (etc)
