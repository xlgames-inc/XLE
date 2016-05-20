// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Types_Forward.h"

typedef enum DXGI_FORMAT DXGI_FORMAT;

namespace RenderCore { namespace Metal_DX11
{
    inline DXGI_FORMAT      AsDXGIFormat(Format format)		{ return DXGI_FORMAT(format); }
    inline Format			AsFormat(DXGI_FORMAT format)	{ return Format(format); }
}}

