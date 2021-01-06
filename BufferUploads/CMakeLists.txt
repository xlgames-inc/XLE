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

list(APPEND Src
    DX11/PlatformInterfaceDX11.cpp)

add_library(BufferUploads STATIC ${Src} ${Headers})
configure_xle_library(BufferUploads)
target_link_libraries(BufferUploads PRIVATE ForeignMisc)