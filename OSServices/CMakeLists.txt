
set(Src
    Socket.cpp
    BasicFile.cpp
    )
set(Headers
    SystemUtils.h
    TimeUtils.h
    )

if (WIN32)
    set(WinAPISrc WinAPI/System_WinAPI.cpp WinAPI/FileSystemMonitor_WinAPI.cpp WinAPI/BasicFile_WinAPI.cpp)
elseif (NOT APPLE)
    set(LinuxSrc Linux/System_Linux.cpp Linux/FileSystemMonitor_Linux.cpp Linux/BasicFile_Linux.cpp Linux/BasicFile_stdio.cpp)
endif()

add_library(OSServices STATIC ${Src} ${Headers} ${WinAPISrc} ${LinuxSrc})
configure_xle_library(OSServices)
target_link_libraries(OSServices PRIVATE ForeignMisc Utility)

if (WIN32)
    target_link_libraries(OSServices PRIVATE wsock32.lib ws2_32.lib)
elseif (NOT APPLE)
    target_link_libraries(OSServices PRIVATE pthread)    
endif ()
