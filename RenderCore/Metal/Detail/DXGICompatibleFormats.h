// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

    //
    //      X Macro include for commonly used DXGI-compatible
    //      formats.
    //
    //      We don't need to include every single DXGI format here.
    //      But it's convenient to use formats that are compatible
    //      with the DXGI list (at least for simple, uncompressed
    //      formats). Some compressed formats 
    //

_EXP( R32G32B32A32,   TYPELESS,  None,    32*4 )
_EXP( R32G32B32A32,   FLOAT,     None,    32*4 )
_EXP( R32G32B32A32,   UINT,      None,    32*4 )
_EXP( R32G32B32A32,   SINT,      None,    32*4 )

_EXP( R32G32B32,      TYPELESS,  None,    32*3 )
_EXP( R32G32B32,      FLOAT,     None,    32*3 )
_EXP( R32G32B32,      UINT,      None,    32*3 )
_EXP( R32G32B32,      SINT,      None,    32*3 )

_EXP( R16G16B16A16,   TYPELESS,   None,   16*4 )
_EXP( R16G16B16A16,   FLOAT,      None,   16*4 )
_EXP( R16G16B16A16,   UNORM,      None,   16*4 )
_EXP( R16G16B16A16,   UINT,       None,   16*4 )
_EXP( R16G16B16A16,   SNORM,      None,   16*4 )
_EXP( R16G16B16A16,   SINT,       None,   16*4 )

_EXP( R32G32,         TYPELESS,   None,   32*2 )
_EXP( R32G32,         FLOAT,      None,   32*2 )
_EXP( R32G32,         UINT,       None,   32*2 )
_EXP( R32G32,         SINT,       None,   32*2 )

    //
    //      These formats don't suit our macros (but aren't very useful)
    //
    //    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    //    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    //    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    //    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    //

_EXP( R10G10B10A2,    TYPELESS,   None,       32 )
_EXP( R10G10B10A2,    UNORM,      None,       32 )
_EXP( R10G10B10A2,    UINT,       None,       32 )
_EXP( R11G11B10,      FLOAT,      None,       32 )

_EXP( R8G8B8A8,       TYPELESS,   None,       8*4 )
_EXP( R8G8B8A8,       UNORM,      None,       8*4 )
_EXP( R8G8B8A8,       UNORM_SRGB, None,       8*4 )
_EXP( R8G8B8A8,       UINT,       None,       8*4 )
_EXP( R8G8B8A8,       SNORM,      None,       8*4 )
_EXP( R8G8B8A8,       SINT,       None,       8*4 )

_EXP( R16G16,         TYPELESS,   None,       16*2 )
_EXP( R16G16,         FLOAT,      None,       16*2 )
_EXP( R16G16,         UNORM,      None,       16*2 )
_EXP( R16G16,         UINT,       None,       16*2 )
_EXP( R16G16,         SNORM,      None,       16*2 )
_EXP( R16G16,         SINT,       None,       16*2 )

_EXP( R32,            TYPELESS,   None,       32 )
_EXP( D32,            FLOAT,      None,       32 )
_EXP( R32,            FLOAT,      None,       32 )
_EXP( R32,            UINT,       None,       32 )
_EXP( R32,            SINT,       None,       32 )

    //
    //      These formats don't suit our macros (but are useful!)
    //
    //    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    //    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    //    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    //    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    //

_EXP( R8G8,           TYPELESS,   None,       8*2 )
_EXP( R8G8,           UNORM,      None,       8*2 )
_EXP( R8G8,           UINT,       None,       8*2 )
_EXP( R8G8,           SNORM,      None,       8*2 )
_EXP( R8G8,           SINT,       None,       8*2 )

_EXP( R16,            TYPELESS,   None,       16 )
_EXP( R16,            FLOAT,      None,       16 )
_EXP( D16,            UNORM,      None,       16 )
_EXP( R16,            UNORM,      None,       16 )
_EXP( R16,            UINT,       None,       16 )
_EXP( R16,            SNORM,      None,       16 )
_EXP( R16,            SINT,       None,       16 )
    
_EXP( R8,             TYPELESS,   None,       8 )
_EXP( R8,             UNORM,      None,       8 )
_EXP( R8,             UINT,       None,       8 )
_EXP( R8,             SNORM,      None,       8 )
_EXP( R8,             SINT,       None,       8 )
_EXP( A8,             UNORM,      None,       8 )
_EXP( R1,             UNORM,      None,       1 )

_EXP( R9G9B9E5,       SHAREDEXP,  None,       32 )
_EXP( R8G8_B8G8,      UNORM,      None,       16 )
_EXP( G8R8_G8B8,      UNORM,      None,       16 )

_EXP( BC1,            TYPELESS,   BlockCompression,   4 )
_EXP( BC1,            UNORM,      BlockCompression,   4 )
_EXP( BC1,            UNORM_SRGB, BlockCompression,   4 )

_EXP( BC2,            TYPELESS,   BlockCompression,   8 )
_EXP( BC2,            UNORM,      BlockCompression,   8 )
_EXP( BC2,            UNORM_SRGB, BlockCompression,   8 )
    
_EXP( BC3,            TYPELESS,   BlockCompression,   8 )
_EXP( BC3,            UNORM,      BlockCompression,   8 )
_EXP( BC3,            UNORM_SRGB, BlockCompression,   8 )

_EXP( BC4,            TYPELESS,   BlockCompression,   8 )
_EXP( BC4,            UNORM,      BlockCompression,   8 )
_EXP( BC4,            SNORM,      BlockCompression,   8 )

_EXP( BC5,            TYPELESS,   BlockCompression,   8 )
_EXP( BC5,            UNORM,      BlockCompression,   8 )
_EXP( BC5,            SNORM,      BlockCompression,   8 )

_EXP( BC6H,           TYPELESS,   BlockCompression,   8 )
_EXP( BC6H,           UF16,       BlockCompression,   8 )
_EXP( BC6H,           SF16,       BlockCompression,   8 )

_EXP( BC7,            TYPELESS,   BlockCompression,   8 )
_EXP( BC7,            UNORM,      BlockCompression,   8 )
_EXP( BC7,            UNORM_SRGB, BlockCompression,   8 )

_EXP( B5G6R5,         UNORM,      None,               16 )
_EXP( B5G5R5A1,       UNORM,      None,               16 )

_EXP( B8G8R8A8,       TYPELESS,   None,               8*4 )
_EXP( B8G8R8A8,       UNORM,      None,               8*4 )
_EXP( B8G8R8A8,       UNORM_SRGB, None,               8*4 )

_EXP( B8G8R8X8,       TYPELESS,   None,               8*4 )
_EXP( B8G8R8X8,       UNORM,      None,               8*4 )
_EXP( B8G8R8X8,       UNORM_SRGB, None,               8*4 )

    //
    //      Some less common types
    //
    //    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    //

