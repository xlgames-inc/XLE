// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FontPrimitives.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/IntrusivePtr.h"
#include <vector>
#include <memory>

namespace RenderCore { class IResource; class Box2D; }
namespace BufferUploads { class IManager; class ResourceLocator; class DataPacket; using TransactionID = uint64_t; }

namespace RenderOverlays
{
	class FontTexture2D
	{
	public:
		void UpdateToTexture(BufferUploads::DataPacket& packet, const RenderCore::Box2D& destBox);
		const std::shared_ptr<RenderCore::IResource>& GetUnderlying() const;
		const RenderCore::Metal::ShaderResourceView& GetSRV() const;

		FontTexture2D(unsigned width, unsigned height, RenderCore::Format pixelFormat);
		~FontTexture2D();

		FontTexture2D(FontTexture2D&&) = default;
		FontTexture2D& operator=(FontTexture2D&&) = default;

	private:
		mutable BufferUploads::TransactionID					_transaction;
		mutable intrusive_ptr<BufferUploads::ResourceLocator>	_locator;
		mutable RenderCore::Metal::ShaderResourceView			_srv;

		void Resolve() const;
	};

	struct FontCharTable
	{
		std::vector<std::vector<std::pair<ucs4, FontBitmapId>>>  _table;
		FontBitmapId&         operator[](ucs4 ch);
		void                ClearTable();
		FontCharTable();
		~FontCharTable();
	};

	class FT_FontTextureMgr
	{
	public:
		// bool            Init(int texWidth, int texHeight);

		// bool            IsNeedReset() const;
		// void            RequestReset();
		// void            Reset();

		// typedef std::vector<std::unique_ptr<CharSlotArray>> CharSlotArrayList;

		struct Glyph
		{
			UInt2 _topLeft = UInt2{0, 0};
			UInt2 _bottomRight = UInt2{0, 0};
			FontBitmapId _glyphId = FontGlyphID_Invalid;
		};

		Glyph		CreateChar(
			unsigned width, unsigned height,
			IteratorRange<const void*> data);

		const FontTexture2D& GetFontTexture();
		UInt2 GetTextureDimensions();
    
		FontCharTable       _table;
			
		FT_FontTextureMgr();
		~FT_FontTextureMgr();

	private:
		// CharSlotArray*      OverwriteLRUChar(FontFace* face);
		// void                ClearFontTextureRegion(int height, int heightEnd);

		class Pimpl;
		std::shared_ptr<Pimpl> _pimpl;
	};

	FT_FontTextureMgr& GetFontTextureMgr();

}

