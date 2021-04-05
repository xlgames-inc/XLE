// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "OverlayPrimitives.h"
#include "../RenderCore/Format.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/IntrusivePtr.h"
#include <vector>
#include <memory>

namespace RenderCore { class IResource; class IResourceView; class Box2D; class IThreadContext; class IDevice; }
namespace BufferUploads { class IManager; class ResourceLocator; class DataPacket; using TransactionID = uint64_t; }

namespace RenderOverlays
{
	class FontTexture2D
	{
	public:
		void UpdateToTexture(RenderCore::IThreadContext& threadContext, IteratorRange<const uint8_t*> data, const RenderCore::Box2D& destBox);
		const std::shared_ptr<RenderCore::IResource>& GetUnderlying() const { return _resource; }
		const std::shared_ptr<RenderCore::IResourceView>& GetSRV() const { return _srv; }

		FontTexture2D(
			RenderCore::IDevice& dev,
			unsigned width, unsigned height, RenderCore::Format pixelFormat);
		~FontTexture2D();

		FontTexture2D(FontTexture2D&&) = default;
		FontTexture2D& operator=(FontTexture2D&&) = default;

	private:
		std::shared_ptr<RenderCore::IResource>			_resource;
		std::shared_ptr<RenderCore::IResourceView>		_srv;
		RenderCore::Format _format;
	};

	class FT_FontTextureMgr
	{
	public:
		struct Glyph
		{
			UInt2 _topLeft = UInt2{0, 0};
			UInt2 _bottomRight = UInt2{0, 0};
			FontBitmapId _glyphId = FontBitmapId_Invalid;
		};

		Glyph		CreateChar(
			RenderCore::IThreadContext& threadContext,
			unsigned width, unsigned height,
			IteratorRange<const void*> data);

		const FontTexture2D& GetFontTexture();
		UInt2 GetTextureDimensions();
    
		FT_FontTextureMgr();
		~FT_FontTextureMgr();

	private:
		class Pimpl;
		std::shared_ptr<Pimpl> _pimpl;
	};

	FT_FontTextureMgr& GetFontTextureMgr();

}

