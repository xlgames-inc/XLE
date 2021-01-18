
set(Src
    Socket.cpp
    RawFS.cpp
    )
set(Headers
    RawFS.h
    TimeUtils.h
    )

if (WIN32)
    set(WinAPISrc WinAPI/System_WinAPI.cpp WinAPI/FileSystemMonitor_WinAPI.cpp WinAPI/RawFS_WinAPI.cpp)
elseif (NOT APPLE)
    set(LinuxSrc Linux/System_Linux.cpp Linux/FileSystemMonitor_Linux.cpp Linux/RawFS_Linux.cpp RawFS_stdio.cpp)
endif()

add_library(OSServices STATIC ${Src} ${Headers} ${WinAPISrc} ${LinuxSrc})
configure_xle_library(OSServices)
target_link_libraries(OSServices PRIVATE ForeignMisc Utility)

if (WIN32)
    target_link_libraries(OSServices PRIVATE wsock32.lib ws2_32.lib)
elseif (NOT APPLE)
    target_link_libraries(OSServices PRIVATE pthread)    
endif ()
