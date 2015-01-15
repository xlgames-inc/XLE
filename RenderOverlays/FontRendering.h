// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FontPrimitives.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../Utility/Mixins.h"

namespace RenderOverlays
{

class FontTexture2D : noncopyable
{
public:
    FontTexture2D(unsigned width, unsigned height, unsigned pixelFormat);
    ~FontTexture2D();

    void*   GetUnderlying() const;
    void    UpdateGlyphToTexture(FT_GlyphSlot glyph, int offX, int offY, int width, int height);
    void    UpdateToTexture(BufferUploads::RawDataPacket* packet, int offX, int offY, int width, int height);

private:
    mutable BufferUploads::TransactionID    _transaction;
    mutable intrusive_ptr<BufferUploads::ResourceLocator>  _locator;
};

}

