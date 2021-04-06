// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_FontTexture.h"
#include "Font.h"
#include "FontRectanglePacking.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Resource.h"
#include "../Core/Types.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include <assert.h>
#include <algorithm>
#include <functional>

namespace RenderOverlays
{

	#pragma warning(disable:4127)

	///////////////////////////////////////////////////////////////////////////////////////////////////

	class FontRenderingManager::Pimpl
	{
	public:
		RectanglePacker_FontCharArray	_rectanglePacker;
		std::unique_ptr<FontTexture2D>  _texture;

		unsigned _texWidth, _texHeight;

		Pimpl(RenderCore::IDevice& device, unsigned texWidth, unsigned texHeight)
		: _rectanglePacker({texWidth, texHeight})
		, _texWidth(texWidth), _texHeight(texHeight) 
		{
			_texture = std::make_unique<FontTexture2D>(device, texWidth, texHeight, RenderCore::Format::R8_UNORM);
		}
	};


	///////////////////////////////////////////////////////////////////////////////////////////////////

	FontRenderingManager::FontRenderingManager(RenderCore::IDevice& device) { _pimpl = std::make_unique<Pimpl>(device, 512, 2048); }
	FontRenderingManager::~FontRenderingManager() {}

	FontTexture2D::FontTexture2D(
		RenderCore::IDevice& dev,
		unsigned width, unsigned height, RenderCore::Format pixelFormat)
	: _format(pixelFormat)
	{
		using namespace RenderCore;
		ResourceDesc desc;
		desc._type = ResourceDesc::Type::Texture;
		desc._bindFlags = BindFlag::ShaderResource | BindFlag::TransferDst;
		desc._cpuAccess = CPUAccess::Write;
		desc._gpuAccess = GPUAccess::Read;
		desc._allocationRules = 0;
		desc._textureDesc = TextureDesc::Plain2D(width, height, pixelFormat, 1);
		XlCopyString(desc._name, "Font");
		_resource = dev.CreateResource(desc);
		_srv = _resource->CreateTextureView();
	}

	FontTexture2D::~FontTexture2D()
	{
	}

	void FontTexture2D::UpdateToTexture(
		RenderCore::IThreadContext& threadContext,
		IteratorRange<const uint8_t*> data, const RenderCore::Box2D& destBox)
	{
		using namespace RenderCore;
		auto& metalContext = *Metal::DeviceContext::Get(threadContext);
		auto blitEncoder = metalContext.BeginBlitEncoder();
		blitEncoder.Write(
			Metal::BlitEncoder::CopyPartial_Dest {
				_resource.get(), {}, VectorPattern<unsigned, 3>{ unsigned(destBox._left), unsigned(destBox._top), 0u }
			},
			SubResourceInitData { data },
			_format,
			VectorPattern<unsigned, 3>{ unsigned(destBox._right - destBox._left), unsigned(destBox._bottom - destBox._top), 1u });
	}

	static std::vector<uint8_t> GlyphAsDataPacket(
		unsigned srcWidth, unsigned srcHeight,
		IteratorRange<const void*> srcData,
		int offX, int offY, int width, int height)
	{
		std::vector<uint8_t> packet(width*height);

		int j = 0;
		for (; j < std::min(height, (int)srcHeight); ++j) {
			int i = 0;
			for (; i < std::min(width, (int)srcWidth); ++i)
				packet[i + j*width] = ((const uint8_t*)srcData.begin())[i + srcWidth * j];
			for (; i < width; ++i)
				packet[i + j*width] = 0;
		}
		for (; j < height; ++j)
			for (int i=0; i < width; ++i)
				packet[i + j*width] = 0;

		return packet;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

	auto FontRenderingManager::InitializeNewGlyph(
		RenderCore::IThreadContext& threadContext,
		const Font& font,
		ucs4 ch,
		std::vector<std::pair<uint64_t, Bitmap>>::iterator insertPoint,
		uint64_t code) -> Bitmap
	{
		auto newData = font.GetBitmap(ch);
		if ((newData._width * newData._height) == 0) {
			Bitmap result = {};
			_glyphs.insert(insertPoint, std::make_pair(code, result));
			return result;
		}

		auto rect = _pimpl->_rectanglePacker.Allocate({newData._width, newData._height});
		if (rect.second[0] <= rect.first[0] || rect.second[1] <= rect.first[1])
			return {};

		if (_pimpl->_texture) {
			auto pkt = GlyphAsDataPacket(newData._width, newData._height, newData._data, rect.first[0], rect.first[1], rect.second[0]-rect.first[0], rect.second[1]-rect.first[1]);
			_pimpl->_texture->UpdateToTexture(
				threadContext,
				pkt, 
				RenderCore::Box2D{
					(int)rect.first[0], (int)rect.first[1], 
					(int)rect.second[0], (int)rect.second[1]});
		}

		assert(rect.second[0] > rect.first[0]);
		assert(rect.second[1] > rect.first[1]);

		Bitmap result;
		result._xAdvance = newData._xAdvance;
		result._bitmapOffsetX = newData._bitmapOffsetX;
		result._bitmapOffsetY = newData._bitmapOffsetY;
		result._width = rect.second[0] - rect.first[0];
		result._height = rect.second[1] - rect.first[1];
		result._tcTopLeft[0] = rect.first[0] / float(_pimpl->_texWidth);
		result._tcTopLeft[1] = rect.first[1] / float(_pimpl->_texHeight);
		result._tcBottomRight[0] = rect.second[0] / float(_pimpl->_texWidth);
		result._tcBottomRight[1] = rect.second[1] / float(_pimpl->_texHeight);

		_glyphs.insert(insertPoint, std::make_pair(code, result));
		return result;
	}

	const FontTexture2D& FontRenderingManager::GetFontTexture()
	{
		return *_pimpl->_texture;
	}

	UInt2 FontRenderingManager::GetTextureDimensions()
	{
		return UInt2{_pimpl->_texWidth, _pimpl->_texHeight};
	}

}

