// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "OverlayPrimitives.h"
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
		struct FontProperties
		{
			float _descender = 0.f, _ascender = 0.f;
			float _ascenderExcludingAccent = 0.f;
			float _lineHeight = 0.f;
			float _maxAdvance = 0.f;
		};

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
			FontBitmapId _textureId = FontBitmapId_Invalid;
		};

		virtual FontProperties		GetFontProperties() const = 0;
		virtual GlyphProperties		GetGlyphProperties(ucs4 ch) const = 0;
		virtual Bitmap				GetBitmap(ucs4 ch) const = 0;

		virtual Float2		GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const = 0;
		virtual float       GetKerning(ucs4 prev, ucs4 ch) const = 0;

		virtual ~Font();
    };

	std::shared_ptr<Font> GetX2Font(StringSection<> path, int size);
	std::shared_ptr<Font> GetDefaultFont(unsigned points=16);

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

    class TextStyle
    {
    public:
		DrawTextOptions			_options;
	};
        
    float       Draw(   RenderCore::IThreadContext& threadContext,
						const Font& font, const TextStyle& style,
                        float x, float y, StringSection<ucs4> text,
                        float spaceExtra, float scale, float mx, float depth,
                        unsigned colorARGB, bool applyDescender, Quad* q);

    Float2		AlignText(const Font& font, const Quad& q, TextAlignment align, StringSection<ucs4> text);
    Float2		AlignText(const Font& font, const Quad& q, TextAlignment align, float width, float indent);

}

