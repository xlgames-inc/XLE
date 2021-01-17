
set(Src
    ArithmeticUtils.cpp
    BitUtils.cpp
    Conversion.cpp
    FastParseValue.cpp
    FunctionUtils.cpp
    HashUtils.cpp
    HeapUtils.cpp
    MiniHeap.cpp
    MiscImplementation.cpp
    ParameterBox.cpp
    StringFormat.cpp
    StringFormatTime.cpp
    StringUtils.cpp
    UTFUtils.cpp
    ImpliedTyping.cpp
    xl_snprintf.cpp)
set(Headers
    ArithmeticUtils.h
    BitHeap.h
    BitUtils.h
    Conversion.h
    Documentation.h
    FastParseValue.h
    FunctionUtils.h
    HeapUtils.h
    IntrusivePtr.h
    IteratorUtils.h
    MemoryUtils.h
    MiniHeap.h
    Optional.h
    ParameterBox.h
    ParameterPackUtils.h
    PtrUtils.h
    StreamUtils.h
    StringFormat.h
    StringUtils.h
    SystemUtils.h
    TimeUtils.h
    UTFUtils.h
    VariantUtils.h)
set(NetworkingSrc Networking/Socket.cpp)
set(StreamsSrc 
    Streams/ConditionalPreprocessingTokenizer.cpp
    Streams/Data.cpp
    Streams/DataSerialize.cpp
    Streams/FileUtils.cpp
    Streams/PathUtils.cpp
    Streams/PreprocessorInterpreter.cpp
    Streams/Stream.cpp
    Streams/StreamDOM.cpp
    Streams/StreamFormatter.cpp
    Streams/XmlStreamFormatter.cpp)
set(ThreadingSrc Threading/CompletionThreadPool.cpp)
set(ProfilingSrc Profiling/CPUProfiler.cpp Profiling/SuppressionProfiler.cpp)
set(MetaSrc Meta/AccessorSerialize.cpp Meta/ClassAccessors.cpp)

if (WIN32)
    set(WinAPISrc WinAPI/System_WinAPI.cpp Streams/WinAPI/FileSystemMonitor_WinAPI.cpp Streams/WinAPI/FileUtils_WinAPI.cpp Threading/WinAPI/ThreadObject_WinAPI.cpp)
elseif (NOT APPLE)
    set(LinuxSrc Linux/System_Linux.cpp Streams/Linux/FileSystemMonitor_Linux.cpp Streams/Linux/FileUtils_Linux.cpp Streams/Linux/FileUtils_stdio.cpp)
endif()

add_library(Utility STATIC ${Src} ${Headers} ${NetworkingSrc} ${StreamsSrc} ${ThreadingSrc} ${WinAPISrc} ${ProfilingSrc} ${MetaSrc} ${LinuxSrc})
configure_xle_library(Utility)
target_link_libraries(Utility PRIVATE ForeignMisc)

if (WIN32)
    target_link_libraries(Utility PRIVATE wsock32.lib ws2_32.lib)
elseif (NOT APPLE)
    target_link_libraries(Utility PRIVATE pthread)    
endif ()
