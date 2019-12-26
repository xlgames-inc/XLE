
set(Src
    ArithmeticUtils.cpp
    BitUtils.cpp
    Conversion.cpp
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
    xl_snprintf.cpp)
set(Headers
    ArithmeticUtils.h
    BitHeap.h
    BitUtils.h
    Conversion.h
    Documentation.h
    ExceptionLogging.h
    ExposeStreamOp.h
    FunctionUtils.h
    HeapUtils.h
    IntrusivePtr.h
    IteratorUtils.h
    MemoryUtils.h
    MiniHeap.h
    Mixins.h
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
set(NetworkingSrc Networking/socket.cpp)
set(StreamsSrc 
    Streams/Data.cpp
    Streams/DataSerialize.cpp
    Streams/FileUtils.cpp
    Streams/PathUtils.cpp
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
    set(LinuxSrc Linux/System_Linux.cpp Streams/Linux/FileSystemMonitor_Linux.cpp Streams/Linux/FileUtils_Linux.cpp)
endif()

BasicLibrary(Utility "${Src};${Headers};${NetworkingSrc};${StreamsSrc};${ThreadingSrc};${WinAPISrc};${ProfilingSrc};${MetaSrc};${LinuxSrc}")
target_link_libraries(Utility ForeignMisc)  # required to pull in interface include directories (etc)

