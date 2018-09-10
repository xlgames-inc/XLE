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
		RectanglePacker_FontCharArray _rectanglePacker;

		std::vector<FontFace>			_faceList;
		std::unique_ptr<FontTexture2D>  _texture;

		std::vector<FontFace::Glyph>	_glyphs;

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

	static intrusive_ptr<BufferUploads::DataPacket> GlyphAsDataPacket(FT_GlyphSlot glyph, int offX, int offY, int width, int height)
	{
		auto packet = BufferUploads::CreateBasicPacket(
			width*height, nullptr, RenderCore::TexturePitches{unsigned(width), unsigned(width*height)});
		uint8* data = (uint8*)packet->GetData();

		int glyphWidth = glyph->bitmap.width;
		int glyphHeight = glyph->bitmap.rows;
		if (glyph->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
			glyphWidth = glyphHeight = 0;

		int j = 0;
		for (; j < std::min(height, glyphHeight); ++j) {
			int i = 0;
			for (; i < std::min(width, glyphWidth); ++i)
				data[i + j*width] = glyph->bitmap.buffer[i + glyph->bitmap.width * j];
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

	FontGlyphID FT_FontTextureMgr::FontFace::CreateChar(int ch)
	{
		FT_Error error = FT_Load_Char(_face, ch, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT);
		if (error)
			return FontGlyphID_Invalid;

		FT_GlyphSlot glyph = _face->glyph;

		Glyph fc{ch};
		fc.left     = (float)glyph->bitmap_left;
		fc.top      = (float)glyph->bitmap_top;
		fc.width    = (float)glyph->bitmap.width;
		fc.height   = (float)glyph->bitmap.rows;
		fc.xAdvance = (float)glyph->advance.x / 64.0f;

		auto rect = _pimpl->_rectanglePacker.Allocate({glyph->bitmap.width, glyph->bitmap.rows});
		if (rect.second[0] <= rect.first[0] || rect.second[1] <= rect.first[1])
			return FontGlyphID_Invalid;

		if (_pimpl->_texture) {
			auto pkt = GlyphAsDataPacket(glyph, rect.first[0], rect.first[1], glyph->bitmap.width, glyph->bitmap.rows);
			_pimpl->_texture->UpdateToTexture(
				*pkt, 
				RenderCore::Box2D{
					(int)rect.first[0], (int)rect.first[1], 
					(int)(rect.second[0] - rect.first[0]), (int)(rect.second[1] - rect.first[1])});
		}

		fc.u0 = (float)rect.first[0] / _pimpl->_texWidth;
		fc.v0 = (float)rect.first[1] / _pimpl->_texHeight;
		fc.u1 = (float)rect.second[0] / _pimpl->_texWidth;
		fc.v1 = (float)rect.second[1] / _pimpl->_texHeight;

		_pimpl->_glyphs.push_back(fc);

		return FontGlyphID(_pimpl->_glyphs.size()-1);
	}

	auto FT_FontTextureMgr::FindFontFace(FT_Face face, int size) -> std::shared_ptr<FontFace>
	{
		auto i = std::find_if(
			_faces.begin(), _faces.end(),
			[face, size](const Entry& e) { return e._ftFace == face && e._faceSize == size; });
		if (i != _faces.end())
			return i->_face.lock();
		return nullptr;
	}

	auto FT_FontTextureMgr::CreateFontFace(FT_Face face, int size) -> std::shared_ptr<FontFace>
	{
		auto i = std::find_if(
			_faces.begin(), _faces.end(),
			[face, size](const Entry& e) { return e._ftFace == face && e._faceSize == size; });

		if (i != _faces.end()) {
			auto res = i->_face.lock();
			if (res)
				return res;
		} else {
			i = _faces.insert(_faces.end(), Entry{ face, size });
		}


		auto fontFace = std::make_shared<FontFace>(face, size, _pimpl);
		i->_face = fontFace;
		return fontFace;
	}

	FT_FontTextureMgr::FontFace::FontFace(FT_Face face, int faceSize, const std::shared_ptr<Pimpl>& pimpl)
	: _face(face), _faceSize(faceSize), _pimpl(pimpl)
	{}

	FT_FontTextureMgr::FontFace::~FontFace()
	{
	}

	auto FT_FontTextureMgr::FontFace::GetChar(int ch) -> const Glyph*
	{
		FontGlyphID& id = _table[ch];
		if (id == FontGlyphID_Invalid)
			id = CreateChar(ch);

		if (id < _pimpl->_glyphs.size())
			return &_pimpl->_glyphs[id];

		return nullptr;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

	#define FONT_TABLE_SIZE (256+1024)

	FT_FontTextureMgr::FontCharTable::FontCharTable()
	{
		_table.resize(FONT_TABLE_SIZE);
	}

	FT_FontTextureMgr::FontCharTable::~FontCharTable()
	{

	}

	void FT_FontTextureMgr::FontCharTable::ClearTable()
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

	FontGlyphID& FT_FontTextureMgr::FontCharTable::operator[](ucs4 ch)
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

