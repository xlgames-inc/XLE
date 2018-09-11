// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FontPrimitives.h"
#include "../RenderCore/IDevice_Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <memory>
#include <utility>

namespace RenderOverlays
{
    class FontTexture2D;

    class Font : public std::enable_shared_from_this<Font>
    {
    public:
        Font();
        virtual ~Font();

        int GetSize()           { return _size; }
        const char* GetPath()   { return _path; }

        // virtual std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const = 0;
        float StringWidth(      const ucs4* text,
                                int maxLen           = -1,
                                float spaceExtra     = 0.0f,
                                bool outline         = false);
        int CharCountFromWidth( const ucs4* text, 
                                float width, 
                                int maxLen           = -1,
                                float spaceExtra     = 0.0f,
                                bool outline         = false);
        float StringEllipsis(   const ucs4* inText,
                                ucs4* outText, 
                                size_t outTextSize,
                                float width,
                                float spaceExtra     = 0.0f,
                                bool outline         = false);
        float CharWidth(ucs4 ch, ucs4 prev) const;

        // virtual FT_Face     GetFace()                   { return nullptr; }
        // virtual FT_Face     GetFace(ucs4 /*ch*/)     { return nullptr; }
        // virtual void        TouchFontChar(const FontChar*)       {}

        virtual float       Descent() const = 0;
        virtual float       Ascent(bool includeAccent) const = 0;
        virtual float       LineHeight() const = 0;
        virtual Float2     GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const = 0;

        // virtual std::shared_ptr<const Font> GetSubFont(ucs4 ch) const;
        // virtual bool        IsMultiFontAdapter() const;

		virtual FontGlyphID GetTextureGlyph(ucs4 ch) const = 0;

		struct GlyphProperties
		{
			float _left, _top;
			float _xAdvance;
		};

		virtual GlyphProperties GetGlyphProperties(ucs4 ch) const = 0;

    protected:
        virtual FontGlyphID  CreateFontChar(ucs4 ch) const = 0;
        virtual void        DeleteFontChar(FontGlyphID fc) = 0;
        virtual float       GetKerning(ucs4 prev, ucs4 ch) const = 0;
    
        char    _path[MaxPath];
        int     _size;
    };

    bool InitFontSystem(RenderCore::IDevice* device, BufferUploads::IManager* bufferUploads);
    void CleanupFontSystem();

    std::shared_ptr<Font> GetX2Font(StringSection<> path, int size);

    struct DrawTextOptions 
    {
        uint32 shadow : 1;
        uint32 snap : 1;
        uint32 outline : 1;
        uint32 colorSetIndex : 1;
        uint32 reserved : 28;

        DrawTextOptions() 
        {
            shadow = 1;
            snap = 1;
            outline = 0;
            colorSetIndex = 0;
        }
        DrawTextOptions(bool iShadow, bool iOutline)
        {
            shadow = iShadow;
            snap = 1;
            outline = iOutline;
            colorSetIndex = 0;
        }
    };

    enum UiAlign 
    {
        UIALIGN_TOP_LEFT = 0,
        UIALIGN_TOP = 1,
        UIALIGN_TOP_RIGHT = 2,
        UIALIGN_LEFT = 3,
        UIALIGN_CENTER = 4,
        UIALIGN_RIGHT = 5,
        UIALIGN_BOTTOM_LEFT = 6,
        UIALIGN_BOTTOM = 7,
        UIALIGN_BOTTOM_RIGHT = 8,
    };

    enum UI_TEXT_STATE {
        UI_TEXT_STATE_NORMAL = 0, 
        UI_TEXT_STATE_REVERSE, 
        UI_TEXT_STATE_INACTIVE_REVERSE, 
    };

    class TextStyle
    {
    public:
        DrawTextOptions			_options;
        unsigned                _pointSize;

        TextStyle(const std::shared_ptr<Font>&, const DrawTextOptions& options = DrawTextOptions());
        TextStyle(unsigned pointSize, const DrawTextOptions& options = DrawTextOptions());
        ~TextStyle();

        float       Draw(   RenderCore::IThreadContext& threadContext, 
                            float x, float y, const ucs4 text[], int maxLen,
                            float spaceExtra, float scale, float mx, float depth,
                            unsigned colorARGB, UI_TEXT_STATE textState, bool applyDescender, Quad* q) const;

        Float2     AlignText(const Quad& q, UiAlign align, const ucs4* text, int maxLen = -1);
        Float2     AlignText(const Quad& q, UiAlign align, float width, float indent);
        float       StringWidth(const ucs4* text, int maxlen = -1);
        int         CharCountFromWidth(const ucs4* text, float width);
        float       SetStringEllipis(const ucs4* inText, ucs4* outText, size_t outTextSize, float width);
        float       CharWidth(ucs4 ch, ucs4 prev);
    };

}

