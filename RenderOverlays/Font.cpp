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

    // DavidJ -- hack! subvert massive virtual call overload by putting in some quick overloads
intrusive_ptr<const Font> Font::GetSubFont(ucs4) const { return this; }
bool Font::IsMultiFontAdapter() const { return false; }

float Font::StringWidth(const ucs4* text, int maxLen, float spaceExtra, bool outline)
{
    if (!text)
        return 0.0f;

    int prevGlyph = 0;

    float x = 0.0f;

        // DavidJ -- Hack -- this function is resulting in virtual call overload. But we
        //                  can simplify by specialising for "FTFontGroup" type implementations
    if (IsMultiFontAdapter()) {
        for (uint32 i = 0; i < (uint32)maxLen; ++i) {
            int ch = text[i];
            if (!ch) break;
            if( ch == '\n') continue;

            int curGlyph;
            intrusive_ptr<const Font> subFont = GetSubFont(ch);
            if (subFont) {
                x += subFont->GetKerning(prevGlyph, ch, &curGlyph)[0];

                const FontChar* chr = subFont->GetChar(ch).first;
                if(chr) {
                    x += chr->xAdvance;

                    if(outline) {
                        x += 2.0f;
                    }
                    if(ch == ' ') {
                        x += spaceExtra;
                    }
                }
                prevGlyph = curGlyph;
            }
        }
    } else {
        for (uint32 i = 0; i < (uint32)maxLen; ++i) {
            int ch = text[i];
            if (!ch) break;
            if( ch == '\n') continue;

            int curGlyph;
            x += GetKerning(prevGlyph, ch, &curGlyph)[0];

            const FontChar* chr = GetChar(ch).first;
            if(chr) {
                x += chr->xAdvance;

                if(outline) {
                    x += 2.0f;
                }
                if(ch == ' ') {
                    x += spaceExtra;
                }
            }
            prevGlyph = curGlyph;
        }
    }

    return x;
}

int Font::CharCountFromWidth(const ucs4* text, float width, int maxLen, float spaceExtra, bool outline)
{
    if (!text)
        return 0;

    int prevGlyph = 0;
    int charCount = 0;

    float x = 0.0f;

    for (uint32 i = 0; i < (uint32)maxLen; ++i) {
        int ch = text[i];
        if (!ch) break;
        if( ch == '\n') continue;

        int curGlyph;
        x += GetKerning(prevGlyph, ch, &curGlyph)[0];
        prevGlyph = curGlyph;

        const FontChar* fc = GetChar(ch).first;
        if(fc) {

            x += fc->xAdvance;

            if(outline) {
                x += 2.0f;
            }
            if(ch == ' ') {
                x += spaceExtra;
            }
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

float Font::StringEllipsis(const ucs4* inText, ucs4* outText, size_t outTextSize, float width, float spaceExtra, bool outline)
{
    if (!inText || !outText)
        return 0.0f;

    if (width <= 0.0f) {
        return 0.0f;
    }

    int prevGlyph = 0;

    float x = 0.0f;

    for (uint32 i = 0 ; i < (uint32)-1 ; ++i) {
        int ch = inText[i];
        if (!ch) break;
        if( ch == '\n') continue;

        int curGlyph;
        x += GetKerning(prevGlyph, ch, &curGlyph)[0];
        prevGlyph = curGlyph;

        const FontChar* fc = GetChar(ch).first;
        if(fc) {

            x += fc->xAdvance;

            if(outline) {
                x += 2.0f;
            }
            if(ch == ' ') {
                x += spaceExtra;
            }
        }

        if (x > width) {
            size_t count = size_t(i);
            if (count > outTextSize - 2) {
                return x;
            }

            CopyString(outText, (int)count, inText);
            outText[count - 1] = '.';
            outText[count] = '.';
            outText[count + 1] = 0;

            return StringWidth(outText, -1, spaceExtra, outline);
        }
    }

    return x;
}

float Font::CharWidth(ucs4 ch, ucs4 prev) const
{
    float x = 0.0f;
    if (prev) {
        x += GetKerning(prev, ch);
    }

    const FontChar* fc = GetChar(ch).first;
    if(fc) {
        x += fc->xAdvance;
    }

    return x;
}

FontChar::FontChar(int ich)
{
    ch = ich;
    u0 = 0.0f;
    v0 = 0.0f;
    u1 = 1.0f;
    v1 = 1.0f;
    left = 0.0f;
    top = 0.0f;
    width = 0.0f;
    height = 0.0f;
    xAdvance = 0.0f;

    offsetX = 0;
    offsetY = 0;

    usedTime = 0; // desktop.time;
    needTexUpdate = false;
}

intrusive_ptr<Font> GetX2Font(const char* path, int size, FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY: 
    case FTK_GENERAL: 
        return GetX2FTFont(path, size, kind);

//    case FTK_IMAGETEXT: 
//        return GetX2ImageTextFont(path, size);
    }

    return nullptr;
}

static float                garbageCollectTime = 0.0f;
BufferUploads::IManager*    gBufferUploads = nullptr;
RenderCore::IDevice*        gRenderDevice = nullptr;

bool InitFontSystem(RenderCore::IDevice* device, BufferUploads::IManager* bufferUploads)
{
    garbageCollectTime = 0.0f;
    gRenderDevice = device;
    gBufferUploads = bufferUploads;

    if(!InitFTFontSystem()) {
        return false;
    }

    // if(!InitImageTextFontSystem()) {
    //     return false;
    // }

    return true;
}

void CleanupFontSystem()
{
    CleanupFTFontSystem();
    // CleanupImageTextFontSystem();
    gBufferUploads = nullptr;
    gRenderDevice = nullptr;
}

void CheckResetFontSystem()
{
    CheckResetFTFontSystem();
}

int GetFontCount(FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY:
    case FTK_GENERAL:
        return GetFTFontCount(kind);

    // case FTK_IMAGETEXT:
    //     return GetImageTextFontCount();
    }

    return 0;
}

int GetFontFileCount()
{
    return GetFTFontFileCount(); // + GetImageTextFontFileCount();
}

}

