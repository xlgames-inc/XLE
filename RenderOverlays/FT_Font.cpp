// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_Font.h"
#include "FT_FontTexture.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../OSServices/RawFS.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Conversion.h"
#include <set>
#include <algorithm>
#include <assert.h>
#include <locale>

#include "ft2build.h"
#include FT_FREETYPE_H

namespace RenderOverlays
{

	//---------------------------------------------------------------------------------------
	//  Font File Buffer Manager
	//      : managing font file chunk buffers
	//---------------------------------------------------------------------------------------
	class FontFileBufferManager
	{
	public:
		::Assets::Blob GetBuffer(const std::string& path)
		{
			auto it = _buffers.find(path);
			if (it != _buffers.end()) {
				auto result = it->second.lock();
				if (result)
					return result;
			}

			auto blob = ::Assets::TryLoadFileAsBlob(path);
			if (blob)
				_buffers.insert(std::make_pair(path, blob));
			return blob;
		}

		FontFileBufferManager() {}
		~FontFileBufferManager() 
		{ 
			for (auto it = _buffers.begin(); it != _buffers.end(); ++it)
				assert(it->second.expired());
		}

	private:
		struct FontFileBuffer
		{
			std::unique_ptr<uint8[]> _buffer;
			size_t _bufferSize = 0;
		};

		using FontBufferMap = std::unordered_map<std::string, std::weak_ptr<std::vector<uint8_t>>>;
		FontBufferMap _buffers;
	};

	class FTFontResources
	{
	public:
		FontFileBufferManager _bufferManager;
		FT_Library _ftLib;
		std::shared_ptr<FT_FontTextureMgr> _fontTexMgr;
		std::unordered_map<std::string, std::string> _nameMap;
		::Assets::DepValPtr _nameMapDepVal;

		FTFontResources();
		~FTFontResources();
	};

	class FTFontResourcesBox
	{
	public:
		std::shared_ptr<FTFontResources> _resources;

		class Desc {};
		FTFontResourcesBox(const Desc&)
		{
			_resources = std::make_shared<FTFontResources>();
		}
		~FTFontResourcesBox() {}
	};

	FTFont::FTFont(StringSection<::Assets::ResChar> faceName, int faceSize)
	{
		_resources = ConsoleRig::FindCachedBox2<FTFontResourcesBox>()._resources;

		std::string finalPath = faceName.AsString();
		auto i = _resources->_nameMap.find(finalPath);
		if (i != _resources->_nameMap.end())
			finalPath = i->second;

		_pBuffer = _resources->_bufferManager.GetBuffer(finalPath);

		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, finalPath);
		::Assets::RegisterAssetDependency(_depVal, _resources->_nameMapDepVal);

		if (!_pBuffer)
			Throw(::Assets::Exceptions::ConstructionError(
				::Assets::Exceptions::ConstructionError::Reason::MissingFile,
				_depVal,
				"Failed to load font (%s)", finalPath.c_str()));

		FT_Face face;
		FT_New_Memory_Face(_resources->_ftLib, _pBuffer->data(), (FT_Long)_pBuffer->size(), 0, &face);
		_face = std::shared_ptr<FT_FaceRec_>{
			face,
			[](FT_Face f) { FT_Done_Face(f); } };

		FT_Error error = FT_Set_Pixel_Sizes(_face.get(), 0, faceSize);
		if (error)
			Throw(::Assets::Exceptions::ConstructionError(
				::Assets::Exceptions::ConstructionError::Reason::FormatNotUnderstood,
				_depVal,
				"Failed to set pixel size while initializing font (%s)", finalPath.c_str()));

		_fontProperties._descender = _face->size->metrics.descender / 64.0f;
		_fontProperties._ascender = _face->size->metrics.ascender / 64.0f;
		_fontProperties._lineHeight = _face->size->metrics.height / 64.0f;
		_fontProperties._maxAdvance = _face->size->metrics.max_advance / 64.0f;
		_fontProperties._ascenderExcludingAccent = _fontProperties._ascender;

		error = FT_Load_Char(_face.get(), 'X', FT_LOAD_RENDER);
		if (!error)
			_fontProperties._ascenderExcludingAccent = (float)_face->glyph->bitmap_top;
	}

	FTFont::~FTFont()
	{
	}

	auto FTFont::GetFontProperties() const -> FontProperties { return _fontProperties; }

	Float2 FTFont::GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const
	{
		int currentGlyph = FT_Get_Char_Index(_face.get(), ch); 
		if(*curGlyph)
			*curGlyph = currentGlyph;

		if (prevGlyph) {
			FT_Vector kerning;
			FT_Get_Kerning(_face.get(), prevGlyph, currentGlyph, FT_KERNING_DEFAULT, &kerning);

			return Float2((float)kerning.x / 64, (float)kerning.y / 64);
		}

		return Float2(0.0f, 0.0f);
	}

	float FTFont::GetKerning(ucs4 prev, ucs4 ch) const
	{
		if (prev) {
			int prevGlyph = FT_Get_Char_Index(_face.get(), prev);
			int curGlyph = FT_Get_Char_Index(_face.get(), ch); 

			FT_Vector kerning;
			FT_Get_Kerning(_face.get(), prevGlyph, curGlyph, FT_KERNING_DEFAULT, &kerning);
			return (float)kerning.x / 64;
		}

		return 0.0f;
	}

	FontBitmapId FTFont::InitializeBitmap(ucs4 ch) const
	{
		FontBitmapId& id = _lookupTable[ch];
		if (id != FontBitmapId_Invalid)
			return id;

		FT_Error error = FT_Load_Char(_face.get(), ch, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT);
		if (error)
			return FontBitmapId_Invalid;

		FT_GlyphSlot glyph = _face->glyph;

		Bitmap glyphEntry;
		glyphEntry._glyph = { (float)glyph->advance.x / 64.0f };
		glyphEntry._bitmapOffsetX = glyph->bitmap_left;
		glyphEntry._bitmapOffsetY = -glyph->bitmap_top;

		if (glyph->bitmap.width!=0 && glyph->bitmap.rows!=0) {
			auto textureGlyph = _resources->_fontTexMgr->CreateChar(
				glyph->bitmap.width, glyph->bitmap.rows,
				MakeIteratorRange(glyph->bitmap.buffer, PtrAdd(glyph->bitmap.buffer, glyph->bitmap.width*glyph->bitmap.rows)));

			glyphEntry._topLeft = textureGlyph._topLeft;
			glyphEntry._bottomRight = textureGlyph._bottomRight;
			glyphEntry._textureId = textureGlyph._glyphId;
		}

		id = (FontBitmapId)_bitmaps.size();
		_bitmaps.push_back(glyphEntry);

		return id;
	}

	auto FTFont::GetBitmap(ucs4 ch) const -> Bitmap
	{
		auto internalTable = InitializeBitmap(ch);
		if (internalTable == FontBitmapId_Invalid) return {};
		return _bitmaps[internalTable];
	}

	auto FTFont::GetGlyphProperties(ucs4 ch) const -> GlyphProperties
	{
		auto internalTable = InitializeBitmap(ch);
		if (internalTable == FontBitmapId_Invalid) return {};
		return _bitmaps[internalTable]._glyph;
	}

	struct FontDef { std::string path; int size; };
	struct FontDefLessPred
	{
		bool operator() (const FontDef& x, const FontDef& y) const
		{
			if (x.size < y.size) {
				return true;
			}

			if (x.size > y.size) {
				return false;
			}

			if (x.path < y.path) {
				return true;
			}

			return false;
		}
	};

	std::shared_ptr<Font> GetX2Font(StringSection<> path, int size)
	{
		auto future = ::Assets::MakeAsset<FTFont>(path, size);
		future->StallWhilePending();
		return future->Actualize();
	}

	std::shared_ptr<Font> GetDefaultFont(unsigned size)
	{
		return GetX2Font("Raleway", size);
	}

	////////////////////////////////////////////////////////////////////////////////////

	static void LoadFontNameMapping(InputStreamFormatter<utf8>& formatter, std::unordered_map<std::string, std::string>& result)
	{
		StringSection<> name;
		while (formatter.TryKeyedItem(name)) {
            switch (formatter.PeekNext()) {
            case FormatterBlob::Value:
				result.insert({name.AsString(), RequireValue(formatter).AsString()});
				break;

			case FormatterBlob::BeginElement:
				RequireBeginElement(formatter);
				SkipElement(formatter);
				RequireEndElement(formatter);
				break;

			default:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));
            }
        }
	}

	static std::unordered_map<std::string, std::string> LoadFontConfigFile(StringSection<> cfgFile)
	{
		std::unordered_map<std::string, std::string> result;

		size_t blobSize = 0;
		auto blob = ::Assets::TryLoadFileAsMemoryBlock(cfgFile, &blobSize);

		InputStreamFormatter<utf8> formatter(MakeStringSection((const char*)blob.get(), (const char*)PtrAdd(blob.get(), blobSize)));

		auto locale = std::locale("").name();

		StringSection<> name;
		while (formatter.TryKeyedItem(name)) {
			RequireBeginElement(formatter);
			if (XlEqStringI(name, "*")) {
				LoadFontNameMapping(formatter, result);
			} else if (XlEqStringI(name, locale)) {
				LoadFontNameMapping(formatter, result);
			} else {
				SkipElement(formatter);
			}
			RequireEndElement(formatter);
        }
	}

	FTFontResources::FTFontResources()
	{
		FT_Error error = FT_Init_FreeType(&_ftLib);
		if (error)
			Throw(::Exceptions::BasicLabel("Freetype font library failed to initialize (error code: %i)", error));

		const char* fontCfg = "xleres/DefaultResources/fonts/fonts.dat";
		_nameMap = LoadFontConfigFile(fontCfg);
		_nameMapDepVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_nameMapDepVal, fontCfg);

		_fontTexMgr = std::make_shared<FT_FontTextureMgr>();
	}

	FTFontResources::~FTFontResources()
	{
		FT_Done_FreeType(_ftLib);
	}

	FT_FontTextureMgr& GetFontTextureMgr()
	{
		return *ConsoleRig::FindCachedBox2<FTFontResourcesBox>()._resources->_fontTexMgr;
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

	FontBitmapId& FontCharTable::operator[](ucs4 ch)
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
		auto i = std::lower_bound(list.begin(), list.end(), ch, CompareFirst<ucs4, FontBitmapId>());
		if (i == list.end() || i->first != ch) {
			auto i2 = list.insert(i, std::make_pair(ch, FontBitmapId_Invalid));
			return i2->second;
		}
		return i->second;
	}

}

