set(Src 
    BufferUploads.cpp
    BufferUploads_Manager.cpp
    DataPacket.cpp
    MemoryManagement.cpp
    PlatformInterface.cpp
    ResourceSource.cpp
    ThreadContext.cpp)
set(Headers 
    BufferUploads_Manager.h
    DataPacket.h
    Documentation.h
    IBufferUploads_Forward.h
    IBufferUploads.h
    MemoryManagement.h
    Metrics.h
    PlatformInterface.h
    ResourceLocator.h
    ResourceSource.h
    ThreadContext.h)

BasicLibrary(BufferUploads "${Src};${Headers};DX11/PlatformInterfaceDX11.cpp")
target_link_libraries(BufferUploads ForeignMisc)  # required to pull in interface include directories (etc)

