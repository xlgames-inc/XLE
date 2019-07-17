file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

BasicLibrary(BufferUploads "${Src};${Headers};DX11/PlatformInterfaceDX11.cpp")

