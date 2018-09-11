// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_FontTexture.h"
#include "FontRectanglePacking.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Assets/Services.h"
#include "../Core/Types.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include <assert.h>
#include <algorithm>
#include <functional>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace RenderOverlays
{

	#pragma warning(disable:4127)

	///////////////////////////////////////////////////////////////////////////////////////////////////

	class FT_FontTextureMgr::Pimpl
	{
	public:
		RectanglePacker_FontCharArray	_rectanglePacker;
		std::unique_ptr<FontTexture2D>  _texture;
		std::vector<Glyph>				_glyphs;

		unsigned _texWidth, _texHeight;

		Pimpl(unsigned texWidth, unsigned texHeight)
		: _rectanglePacker({texWidth, texHeight})
		, _texWidth(texWidth), _texHeight(texHeight) 
		{
			_texture = std::make_unique<FontTexture2D>(texWidth, texHeight, RenderCore::Format::R8_UNORM);
		}
	};


	///////////////////////////////////////////////////////////////////////////////////////////////////

	FT_FontTextureMgr::FT_FontTextureMgr() { _pimpl = std::make_unique<Pimpl>(512, 2048); }
	FT_FontTextureMgr::~FT_FontTextureMgr() {}

	static BufferUploads::IManager& GetBufferUploads()
	{
		return RenderCore::Assets::Services::GetBufferUploads();
	}

	FontTexture2D::FontTexture2D(unsigned width, unsigned height, RenderCore::Format pixelFormat)
	{
		using namespace BufferUploads;
		BufferDesc desc;
		desc._type = BufferDesc::Type::Texture;
		desc._bindFlags = BindFlag::ShaderResource | BindFlag::TransferDst;
		desc._cpuAccess = CPUAccess::Write;
		desc._gpuAccess = GPUAccess::Read;
		desc._allocationRules = 0;
		desc._textureDesc = TextureDesc::Plain2D(width, height, pixelFormat, 1);
		XlCopyString(desc._name, "Font");
		_transaction = GetBufferUploads().Transaction_Begin(
			desc, (BufferUploads::DataPacket*)nullptr, BufferUploads::TransactionOptions::ForceCreate|BufferUploads::TransactionOptions::LongTerm);
	}

	FontTexture2D::~FontTexture2D()
	{
		if (_transaction != ~BufferUploads::TransactionID(0x0)) {
			GetBufferUploads().Transaction_End(_transaction); 
			_transaction = ~BufferUploads::TransactionID(0x0);
		}
	}

	static intrusive_ptr<BufferUploads::DataPacket> GlyphAsDataPacket(
		unsigned srcWidth, unsigned srcHeight,
		IteratorRange<const void*> srcData,
		int offX, int offY, int width, int height)
	{
		auto packet = BufferUploads::CreateBasicPacket(
			width*height, nullptr, RenderCore::TexturePitches{unsigned(width), unsigned(width*height)});
		uint8* data = (uint8*)packet->GetData();

		int j = 0;
		for (; j < std::min(height, (int)srcHeight); ++j) {
			int i = 0;
			for (; i < std::min(width, (int)srcWidth); ++i)
				data[i + j*width] = ((const uint8_t*)srcData.begin())[i + srcWidth * j];
			for (; i < width; ++i)
				data[i + j*width] = 0;
		}
		for (; j < height; ++j)
			for (int i=0; i < width; ++i)
				data[i + j*width] = 0;

		return packet;
	}

	void FontTexture2D::UpdateToTexture(BufferUploads::DataPacket& packet, const RenderCore::Box2D& destBox)
	{
		if (_transaction == ~BufferUploads::TransactionID(0x0)) {
			_transaction = GetBufferUploads().Transaction_Begin(_locator);
		}

		GetBufferUploads().UpdateData(_transaction, &packet, destBox);
	}

	const RenderCore::ResourcePtr& FontTexture2D::GetUnderlying() const
	{
		if ((!_locator || _locator->IsEmpty()) && _transaction) {
			if (GetBufferUploads().IsCompleted(_transaction)) {
				_locator = GetBufferUploads().GetResource(_transaction);
				if (_locator && !_locator->IsEmpty()) {
					GetBufferUploads().Transaction_End(_transaction);
					_transaction = ~BufferUploads::TransactionID(0x0);
				}
			}
		}
		static RenderCore::ResourcePtr nullResPtr;
		return _locator ? _locator->GetUnderlying() : nullResPtr;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

	auto FT_FontTextureMgr::CreateChar(
		unsigned width, unsigned height,
		IteratorRange<const void*> data) -> Glyph
	{
		auto rect = _pimpl->_rectanglePacker.Allocate({width, height});
		if (rect.second[0] <= rect.first[0] || rect.second[1] <= rect.first[1])
			return {};

		if (_pimpl->_texture) {
			auto pkt = GlyphAsDataPacket(width, height, data, rect.first[0], rect.first[1], rect.second[0]-rect.first[0], rect.second[1]-rect.first[1]);
			_pimpl->_texture->UpdateToTexture(
				*pkt, 
				RenderCore::Box2D{
					(int)rect.first[0], (int)rect.first[1], 
					(int)(rect.second[0] - rect.first[0]), (int)(rect.second[1] - rect.first[1])});
		}

		Glyph result;
		result._glyphId = FontGlyphID(_pimpl->_glyphs.size()-1);
		result._topLeft[0] = (float)rect.first[0] / _pimpl->_texWidth;
		result._topLeft[1] = (float)rect.first[1] / _pimpl->_texHeight;
		result._bottomRight[0] = (float)rect.second[0] / _pimpl->_texWidth;
		result._bottomRight[1] = (float)rect.second[1] / _pimpl->_texHeight;

		_pimpl->_glyphs.push_back(result);

		return result;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

	#define FONT_TABLE_SIZE (256+1024)

	FontCharTable::FontCharTable()
	{
		_table.resize(FONT_TABLE_SIZE);
	}

	FontCharTable::~FontCharTable()
	{

	}

	void FontCharTable::ClearTable()
	{
		_table.clear();
	}

	static inline unsigned TableEntryForChar(ucs4 ch)
	{
		unsigned a =  ch &  (1024-1);
		unsigned b = (ch & ~(1024-1))/683;
		unsigned entry = (a^b)&(1024-1);
		entry += (b!=0)*256;
		return entry;
	}

	FontGlyphID& FontCharTable::operator[](ucs4 ch)
	{
			//
			//      DavidJ --   Simple hashing method optimised for when the input is
			//                  largely ASCII characters and Korean characters.
			//                  It should work fine for other unicode character
			//                  sets as well (so long as the character values are
			//                  generally 16 bits long).
			//                  Maps 16 bit unicode value onto a value between 0 and (1024+256)
			//                  
		unsigned entry = TableEntryForChar(ch);
		entry = entry%unsigned(_table.size());
    
		auto& list = _table[entry];
		auto i = std::lower_bound(list.begin(), list.end(), ch, CompareFirst<ucs4, FontGlyphID>());
		if (i == list.end() || i->first != ch) {
			auto i2 = list.insert(i, std::make_pair(ch, FontGlyphID_Invalid));
			return i2->second;
		}
		return i->second;
	}

}

