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
#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <memory>
#include <utility>

namespace RenderOverlays
{
    class Font
    {
    public:
        Font();
        virtual ~Font();

        int GetSize()           { return _size; }
        const char* GetPath()   { return _path; }

        // virtual std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const = 0;

        // virtual FT_Face     GetFace()                   { return nullptr; }
        // virtual FT_Face     GetFace(ucs4 /*ch*/)     { return nullptr; }
        // virtual void        TouchFontChar(const FontChar*)       {}

        virtual float       Descent() const = 0;
        virtual float       Ascent(bool includeAccent) const = 0;
        virtual float       LineHeight() const = 0;
        virtual Float2		GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const = 0;
		virtual float       GetKerning(ucs4 prev, ucs4 ch) const = 0;

        // virtual std::shared_ptr<const Font> GetSubFont(ucs4 ch) const;
        // virtual bool        IsMultiFontAdapter() const;

		struct GlyphProperties
		{
			float _xAdvance = 0.f;
		};

		struct Bitmap
		{
			GlyphProperties _glyph;
			signed _bitmapOffsetX = 0, _bitmapOffsetY = 0;
			UInt2 _topLeft = UInt2{0u,0u};				// coordinates in the texture manager
			UInt2 _bottomRight = UInt2{0u,0u};			// coordinates in the texture manager
			FontBitmapId _textureId = ~FontBitmapId(0);
		};

		virtual GlyphProperties GetGlyphProperties(ucs4 ch) const = 0;
		virtual Bitmap GetBitmap(ucs4 ch) const = 0;

    protected:
        // virtual FontBitmapId  CreateFontChar(ucs4 ch) const = 0;
        // virtual void        DeleteFontChar(FontBitmapId fc) = 0;
    
        char    _path[MaxPath];
        int     _size;
    };

	float CharWidth(		const Font& font, ucs4 ch, ucs4 prev);

	template<typename CharType>
		float StringWidth(      const Font& font,
								StringSection<CharType> text,
								float spaceExtra     = 0.0f,
								bool outline         = false);

    template<typename CharType>
		int CharCountFromWidth( const Font& font,
								StringSection<CharType> text, 
								float width, 
								float spaceExtra     = 0.0f,
								bool outline         = false);

    template<typename CharType>
		float StringEllipsis(   const Font& font,
								StringSection<CharType> inText,
								CharType* outText, 
								size_t outTextSize,
								float width,
								float spaceExtra     = 0.0f,
								bool outline         = false);

    /*bool InitFontSystem(RenderCore::IDevice* device, BufferUploads::IManager* bufferUploads);
    void CleanupFontSystem();*/

    std::shared_ptr<Font> GetX2Font(StringSection<> path, int size);
	std::shared_ptr<Font> GetDefaultFont(unsigned points=32);

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

    enum UI_TEXT_STATE 
	{
        UI_TEXT_STATE_NORMAL = 0, 
        UI_TEXT_STATE_REVERSE, 
        UI_TEXT_STATE_INACTIVE_REVERSE, 
    };

    class TextStyle
    {
    public:
		DrawTextOptions			_options;
	};
        
    float       Draw(   RenderCore::IThreadContext& threadContext,
						const Font& font, const TextStyle& style,
                        float x, float y, StringSection<ucs4> text,
                        float spaceExtra, float scale, float mx, float depth,
                        unsigned colorARGB, UI_TEXT_STATE textState, bool applyDescender, Quad* q);

    Float2		AlignText(const Font& font, const Quad& q, UiAlign align, StringSection<ucs4> text);
    Float2		AlignText(const Font& font, const Quad& q, UiAlign align, float width, float indent);

}

