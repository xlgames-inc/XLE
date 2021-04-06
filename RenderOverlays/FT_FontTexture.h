// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Font.h"
#include "OverlayPrimitives.h"
#include "../RenderCore/Format.h"
#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/MemoryUtils.h"
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

	class Font;

	class FontRenderingManager
	{
	public:
		struct Bitmap
		{
			float _xAdvance = 0.f;
			signed _bitmapOffsetX = 0, _bitmapOffsetY = 0;
			unsigned _width, _height;
			Float2 _tcTopLeft = Float2{0.f, 0.f};
			Float2 _tcBottomRight = Float2{0.f, 0.f};
		};

		Bitmap GetBitmap(
			RenderCore::IThreadContext& threadContext,
			const Font& font,
			ucs4 ch);

		const FontTexture2D& GetFontTexture();
		UInt2 GetTextureDimensions();
    
		FontRenderingManager(RenderCore::IDevice& device);
		~FontRenderingManager();

	private:
		std::vector<std::pair<uint64_t, Bitmap>> _glyphs;
		
		class Pimpl;
		std::shared_ptr<Pimpl> _pimpl;

		Bitmap InitializeNewGlyph(
			RenderCore::IThreadContext& threadContext,
			const Font& font,
			ucs4 ch,
			std::vector<std::pair<uint64_t, Bitmap>>::iterator insertPoint,
			uint64_t code);
	};

	inline auto FontRenderingManager::GetBitmap(
		RenderCore::IThreadContext& threadContext,
		const Font& font,
		ucs4 ch) -> Bitmap
	{
		auto code = HashCombine(ch, font.GetHash());
		auto i = LowerBound(_glyphs, code);
		if (__builtin_expect(i != _glyphs.end() && i->first == code, true))
			return i->second;

		return InitializeNewGlyph(threadContext, font, ch, i, code);
	}

}

