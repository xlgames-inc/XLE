// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Font.h"
#include "FT_Font.h"
#include <assert.h>

namespace RenderOverlays
{

Font::Font()
{
    _path[0] = 0;
    _size = 0;
}

Font::~Font()
{
}

float StringWidth(const Font& font, StringSection<ucs4> text, float spaceExtra, bool outline)
{
    int prevGlyph = 0;
    float x = 0.0f, prevMaxX = 0.0f;
    for (int ch:text) {
        if (ch == '\n') {
			prevMaxX = std::max(x, prevMaxX);
			prevGlyph = 0;
			x = 0;
			continue;
		}

        int curGlyph;
        x += font.GetKerning(prevGlyph, ch, &curGlyph)[0];

        auto chr = font.GetGlyphProperties(ch);
        x += chr._xAdvance;

        if(outline) {
            x += 2.0f;
        }
        if(ch == ' ') {
            x += spaceExtra;
        }
        prevGlyph = curGlyph;
    }

    return std::max(x, prevMaxX);
}

int CharCountFromWidth(const Font& font, StringSection<ucs4> text, float width, float spaceExtra, bool outline)
{
    int prevGlyph = 0;
    int charCount = 0;

    float x = 0.0f;
    for (int ch:text) {
        if (ch == '\n') {
			prevGlyph = 0;
			x = 0;
			continue;
		}

        int curGlyph;
        x += font.GetKerning(prevGlyph, ch, &curGlyph)[0];
        prevGlyph = curGlyph;

        auto chr = font.GetGlyphProperties(ch);
        x += chr._xAdvance;

        if(outline) {
            x += 2.0f;
        }
        if(ch == ' ') {
            x += spaceExtra;
        }

        if (width < x) {
            return charCount;
        }

        ++charCount;
    }

    return charCount;
}

#pragma warning(disable:4706)   // C4706: assignment within conditional expression

static void CopyString(ucs4* dst, int count, const ucs4* src)
{
    if (!count)
        return;

    if (!src) {
        *dst = 0;
        return;
    }

    while (--count && (*dst++ = *src++))
        ;
    *dst = 0;
}

float StringEllipsis(const Font& font, StringSection<ucs4> inText, ucs4* outText, size_t outTextSize, float width, float spaceExtra, bool outline)
{
    if (width <= 0.0f)
        return 0.0f;

    int prevGlyph = 0;
    float x = 0.0f;
    for (auto i=inText.begin(); i!=inText.end(); ++i) {
        int ch = *i;
        if (ch == '\n') {
			prevGlyph = 0;
			x = 0.0f;
			continue;
		}

        int curGlyph;
        x += font.GetKerning(prevGlyph, ch, &curGlyph)[0];
        prevGlyph = curGlyph;

        auto chr = font.GetGlyphProperties(ch);
        x += chr._xAdvance;

        if(outline) {
            x += 2.0f;
        }
        if(ch == ' ') {
            x += spaceExtra;
        }

        if (x > width) {
            size_t count = size_t(i - inText.begin());
            if (count > outTextSize - 2) {
                return x;
            }

            CopyString(outText, (int)count, inText.begin());
            outText[count - 1] = '.';
            outText[count] = '.';
            outText[count + 1] = 0;

            return StringWidth(font, MakeStringSection(outText, &outText[count + 1]), spaceExtra, outline);
        }
    }

    return x;
}

float CharWidth(const Font& font, ucs4 ch, ucs4 prev)
{
    float x = 0.0f;
    if (prev) {
        x += font.GetKerning(prev, ch);
    }

    x += font.GetGlyphProperties(ch)._xAdvance;

    return x;
}

std::shared_ptr<Font> GetX2Font(StringSection<> path, int size)
{
    return GetX2FTFont(path, size);
}

// static float                garbageCollectTime = 0.0f;
// BufferUploads::IManager*    gBufferUploads = nullptr;
// RenderCore::IDevice*        gRenderDevice = nullptr;

bool InitFontSystem(RenderCore::IDevice* device, BufferUploads::IManager* bufferUploads)
{
    // garbageCollectTime = 0.0f;
    // gRenderDevice = device;
    // gBufferUploads = bufferUploads;

    /*if(!InitFTFontSystem()) {
        return false;
    }*/

    // if(!InitImageTextFontSystem()) {
    //     return false;
    // }

    return true;
}

void CleanupFontSystem()
{
    // CleanupFTFontSystem();
    // CleanupImageTextFontSystem();
    // gBufferUploads = nullptr;
    // gRenderDevice = nullptr;
}

}

