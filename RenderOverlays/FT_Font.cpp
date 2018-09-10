// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_Font.h"
#include "FT_FontTexture.h"
#include "../Assets/IFileSystem.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Data.h"
#include <set>
#include <algorithm>
#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace RenderOverlays
{

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


// using UiFontGroupMap = std::unordered_map<FontDef, std::shared_ptr<FTFontGroup>, FontDefLessPred>;     // awkwardly, these can't use smart ptrs... because these objects are removed from the maps in their destructors
// using UiFontMap = std::unordered_map<FontDef, std::shared_ptr<FTFont>, FontDefLessPred>;

// static UiFontGroupMap   fontGroupMap;
// static UiFontGroupMap   damageDisplayFontGroupMap;
// static UiFontMap        fontMap;

// FT_Library ftLib = 0;

// static std::unique_ptr<FT_FontTextureMgr>       fontTexMgr = NULL;
// static std::unique_ptr<FT_FontTextureMgr>       damageDisplayFontTexMgr = NULL;
// static std::unique_ptr<FontFileBufferManager>   fontFileBufferManager = NULL;

class FTFontResources
{
public:
	FontFileBufferManager _bufferManager;
	FT_Library _ftLib;
	std::unique_ptr<FT_FontTextureMgr> _fontTexMgr;

	FTFontResources();
	~FTFontResources();
};

static FTFontResources s_res;

FTFont::FTFont()
{
    _ascend = 0;
    _face = 0;
    _pBuffer = 0;
}

FTFont::~FTFont()
{
}

bool FTFont::Init(const FontDef& fontDef)
{
    FT_Error error;

    XlCopyString(_path, fontDef.path.c_str());

    _size = fontDef.size;

    _pBuffer = s_res._bufferManager.GetBuffer(_path);

    if (!_pBuffer)
        Throw(::Exceptions::BasicLabel("Failed to load font (%s)", fontDef.path.c_str()));

	FT_Face face;
    FT_New_Memory_Face(s_res._ftLib, _pBuffer->data(), (FT_Long)_pBuffer->size(), 0, &face);
	_face = std::shared_ptr<FT_FaceRec_>{
		face,
		[](FT_Face f) { FT_Done_Face(f); } };

    error = FT_Set_Pixel_Sizes(_face.get(), 0, _size);
    if (error) {
        // GameWarning("Failed to set pixel size");
    }

    error = FT_Load_Char(_face.get(), ' ', FT_LOAD_RENDER);
    if (error) {
        // GameWarning("There is no blank character(%s)", _path);
    }

    error = FT_Load_Char(_face.get(), 'X', FT_LOAD_RENDER);
    if (error) {
        // GameWarning("Failed to load character '%d'", 'X');
        return false;
    }

    //bool italic = true;
    //if (italic) {
    //    FT_Matrix matrix;
    //    const float angle = (-gf_PI * 30.0f) / 180.0f;
    //    matrix.xx = (FT_Fixed)0x10000;
    //    matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
    //    matrix.yx = (FT_Fixed)0;
    //    matrix.yy = (FT_Fixed)0x10000;        
    //    FT_Set_Transform(_face,&matrix,0);
    //}


    FT_GlyphSlot slot = _face->glyph;
    _ascend = slot->bitmap_top;

    return true;
}

FontGlyphID FTFont::CreateFontChar(ucs4 ch) const
{
	// return _textureFace->GetChar(ch);
	return FontGlyphID_Invalid;
}

void FTFont::DeleteFontChar(FontGlyphID fc)
{
	assert(0);
}

float FTFont::Descent() const
{
    if (!_face) return 1.0f;
    //float yScale = desktop.heightScale; //
    float yScale = 1;
    return -yScale * _face->size->metrics.descender / 64.0f;
}

float FTFont::Ascent(bool includeAccent) const
{
    if (!_face) return 1.0f;
    //float yScale = desktop.heightScale;
    float yScale = 1;
    if (includeAccent) {
        return yScale * _face->size->metrics.ascender / 64.0f;
    } else {
        return yScale * _ascend;
    }
}

float FTFont::LineHeight() const
{
    if (!_face) return 1.0f;

    //float yScale = desktop.heightScale;
    float yScale = 1;
    return yScale * _face->size->metrics.height / 64.0f;
}

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



#if 0
// static FTFontNameMap fontNameInfo;

FTFontGroup::FTFontGroup(FontTexKind kind)
{
    _ascend = 0;
    _face = 0;
    _pBuffer = 0;
    _texKind = kind;
    _defaultFTFont = nullptr;
}

FTFontGroup::~FTFontGroup()
{
}

bool FTFontGroup::CheckMyFace(FT_Face face)
{
    if (_defaultFTFont && _defaultFTFont->GetFace() == face) {
        return true;
    } else {
        SubFTFontInfoMap::iterator it_begin = _subFTFontInfoMap.begin();
        SubFTFontInfoMap::iterator it_end = _subFTFontInfoMap.end();
        for (; it_begin != it_end; ++it_begin) {
            const auto& subFTFont = it_begin->first;
            if (subFTFont && subFTFont->GetFace() == face)
                return true;
        }
    }

    return false;
}

std::shared_ptr<FTFont> FTFontGroup::FindFTFontByChar(ucs4 ch) const
{
    size_t size = 0;
    FTFontRange range;
    if (_defaultFTFont) {
        size = _defaultFTFontRanges.size();
        for (int i = 0; i < (int)size; ++i) {
            range = _defaultFTFontRanges[i];
            if (range.from <= ch && range.to >= ch) {
                return _defaultFTFont;
            }
        }
    }

    SubFTFontInfoMap::const_iterator it_begin = _subFTFontInfoMap.begin();
    SubFTFontInfoMap::const_iterator it_end = _subFTFontInfoMap.end();
    FTFontRanges ranges;
    for (; it_begin != it_end; ++it_begin) {
        const auto& subFTFont = it_begin->first;
        ranges = (FTFontRanges)(it_begin->second);
        if (subFTFont) {
            size = ranges.size();
            for (int i = 0; i < (int)size; ++i) {
                range = ranges[i];
                if (range.from <= ch && range.to >= ch) {
                    return subFTFont;
                }
            }
        }
    }

    return nullptr;
}

int FTFontGroup::GetFTFontCount()
{
    int count = 0;
    if (_defaultFTFont) {
        ++count;
    }

    count += (int)_subFTFontInfoMap.size();

    return count;
}

bool FTFontGroup::LoadDefaultFTFont(FTFontNameInfo &info, int size)
{
    FontDef fd = {info.defaultFTFontPath.c_str(), size};

    UiFontMap::iterator it = fontMap.find(fd);
    if (it != fontMap.end()) {
        _defaultFTFont = it->second;
    } else {
        _defaultFTFont = std::make_shared<FTFont>(_texKind);

        if (!_defaultFTFont->Init(fd)) {
            _defaultFTFont.reset();
            // GameWarning("Failed to create freetype font '%s'", fd.path.c_str());
            return false;
        }

		fontMap.insert(std::make_pair(fd, _defaultFTFont));
    }

    if (_defaultFTFont) {
        _defaultFTFontRanges = info.defaultFTFontRange;
    }

    return true;
}

void FTFontGroup::LoadSubFTFont(FTFontNameInfo &info, int size)
{
    FontDef fd;
    std::shared_ptr<FTFont> font;
    FTFontInfoMap::iterator it_begin = info.subFTFontInfo.begin();
    FTFontInfoMap::iterator it_end = info.subFTFontInfo.end();
    for (; it_begin != it_end; ++it_begin) {
        font = nullptr;
        fd.path = ((std::string)(it_begin->first)).c_str();
        fd.size = size;

        UiFontMap::iterator it = fontMap.find(fd);
        if (it != fontMap.end()) {
            font = it->second;
        } else {
            font = std::make_shared<FTFont>(_texKind);
            if (!font->Init(fd)) {
                font = nullptr;
                // GameWarning("Failed to create freetype font '%s'", fd.path.c_str());
			} else {
				fontMap.insert(std::make_pair(fd, font));
			}
        }
        
        if (font) {
            _subFTFontInfoMap.insert(SubFTFontInfoMap::value_type(font.get(), (FTFontRanges)it_begin->second));
        }
    }
}

std::pair<const FontChar*, const FontTexture2D*> FTFontGroup::GetChar(ucs4 ch) const
{
    auto font = FindFTFontByChar(ch);
    if (font) {
        return font->GetChar(ch);
    }

    return std::pair<const FontChar*, const FontTexture2D*>(nullptr, nullptr);
}

FT_Face FTFontGroup::GetFace()
{
    if (_defaultFTFont) {
        return _defaultFTFont->GetFace();
    }

    return NULL;
}

FT_Face FTFontGroup::GetFace(ucs4 ch)
{
    auto font = FindFTFontByChar(ch);
    if (font) {
        return font->GetFace();
    }

    return NULL;
}

float FTFontGroup::Descent()
{
    if (_defaultFTFont) {
        return _defaultFTFont->Descent();
    }

    return 1.0f;
}

float FTFontGroup::Ascent(bool includeAccent) const
{
    if (_defaultFTFont) {
        return _defaultFTFont->Ascent(includeAccent);
    }

    return 1.0f;
}

float FTFontGroup::LineHeight() const
{
    if (_defaultFTFont) {
        return _defaultFTFont->LineHeight();
    }

    return 1.0f;
}

Float2 FTFontGroup::GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const
{
    auto font = FindFTFontByChar(ch);
    if (font) {
        return font->GetKerning(prevGlyph, ch, curGlyph);
    }

    return Float2(0.f, 0.f);
}

bool FTFontGroup::Init(const FontDef& def)
{
    XlCopyString(_path, def.path.c_str());
    _size = def.size;

    FTFontNameMap::iterator it = fontNameInfo.find(_path);
    if (it != fontNameInfo.end()) {
        FTFontNameInfo info = (FTFontNameInfo)it->second;
        if (LoadDefaultFTFont(info, _size)) {
            LoadSubFTFont(info, _size);
            return true;
        }
    } else {
        FTFontNameInfo info;
        info.defaultFTFontPath = _path;
        info.defaultFTFontRange.push_back(FTFontRange::Create(0, 0xffff));
        fontNameInfo.insert(FTFontNameMap::value_type(_path, info));
        if (LoadDefaultFTFont(info, _size)) {
            LoadSubFTFont(info, _size);
            return true;
        }
    }

    return false;
}

FontGlyphID FTFontGroup::CreateFontChar(ucs4 ch)
{
	auto font = FindFTFontByChar(ch);
    if (font) {
        return font->CreateFontChar(ch);
    }

    return FontGlyphID(0);
}

float FTFontGroup::GetKerning(ucs4 prev, ucs4 ch) const
{
	auto font = FindFTFontByChar(ch);
    if (font) {
        return font->GetKerning(prev, ch);
    }

    return 0.0f;
}

    // DavidJ -- hack! subvert massive virtual call overload by putting in some quick overloads
std::shared_ptr<const Font> FTFontGroup::GetSubFont(ucs4 ch) const
{
    return FindFTFontByChar(ch);      // DavidJ -- warning -- lots of redundant AddRef/Releases involved with this call
}

bool FTFontGroup::IsMultiFontAdapter() const { return true; }

#endif 

/*
static std::shared_ptr<FTFontGroup> LoadFTFont(const FontDef& def)
{
    auto fontGroup = std::make_shared<FTFontGroup>();
    if (!fontGroup->Init(def)) {
        fontGroup = nullptr;
        // GameWarning("Failed to create freetype font '%s'", path);
    }

    return fontGroup;
}

std::shared_ptr<FTFont> GetX2FTFont(StringSection<> path, int size)
{
    FontDef fd;
    fd.path = path;
    fd.size = size;

    UiFontGroupMap* curFontGroupMap = NULL;
    switch (kind) {
    case FTK_DAMAGEDISPLAY: 
        curFontGroupMap = &damageDisplayFontGroupMap; 
        break;

    case FTK_GENERAL: 
    default:
        curFontGroupMap = &fontGroupMap;
        break;
    }

    UiFontGroupMap::iterator it = curFontGroupMap->find(fd);
    if (it != curFontGroupMap->end()) {
        return it->second;
    }

	auto result = LoadFTFont(fd, kind);
	curFontGroupMap->insert(std::make_pair(fd, result));
	return result;
}*/

static bool LoadDataFromPak(const char* path, Data* out)
{
	size_t size = 0;
    auto str = ::Assets::TryLoadFileAsMemoryBlock(path, &size);
	if (!size) return false;

	return out->Load((const char*)str.get(), (int)size);
}

bool LoadFontConfigFile()
{
	FTFontNameMap fontNameInfo;		// NOTE <-- previously this was filescope static

    Data config;    
    if (!LoadDataFromPak("xleres/DefaultResources/fonts/fonts.g", &config)) {
        return false;
    }
    
    const char* locale = XlGetLocaleString(XlGetLocale());
    Data * localData = config.ChildWithValue(locale);
    if (!localData) {
        return false;
    }

    typedef unsigned short WORD;

    int from, to;
    // [temp], [final] is used to check same value;
    std::set<WORD> temp;
    std::pair<std::set<WORD>::iterator, bool> resultTo;
    std::map<WORD, WORD> final;
    std::pair<std::map<WORD, WORD>::iterator, bool> resultFrom;

    int fontGroupCount = localData->Size();
    for (int i = 0; i < fontGroupCount; ++i) {
        Data * fontGroup = localData->ChildAt(i);
        if (fontGroup) {
            FTFontNameInfo info;
            char* fontGroupName = fontGroup->value;
            info.defaultFTFontPath = fontGroup->ValueAt(0);

            int fontCount = fontGroup->Size();
            for (int j = 1; j < fontCount; ++j) {
                Data * subFontData = fontGroup->ChildAt(j);
                if (subFontData) {
                    char* subFTFontPath = subFontData->value;

                    int rangeCount = subFontData->Size();
                    if (rangeCount % 2 == 0) {
                        FTFontRanges ranges;
                        for (int k = 0; k < rangeCount; ++k) {
                            XlSafeAtoi(subFontData->ValueAt(k), &from);
                            ++k;
                            XlSafeAtoi(subFontData->ValueAt(k), &to);

                            // check that [from] is larger than [to]
                            if (from <= to) {
                                resultTo = temp.insert((WORD)to);
                                // if [resultTo.second] is false, exist equal [to]
                                if ((bool)(resultTo.second) != false) {
                                    resultFrom = final.insert(std::map<WORD, WORD>::value_type((WORD)from, (WORD)to));
                                    // if [resultFrom.second] is false, exist equal [from]
                                    if ((bool)(resultFrom.second) != false) {
                                        ranges.push_back(FTFontRange::Create((WORD)from, (WORD)to));
                                    }
                                }
                            }
                        }

                        if (ranges.size() != 0) {
                            info.subFTFontInfo.insert(FTFontInfoMap::value_type(subFTFontPath, ranges));
                        }
                    }
                }
            }
            temp.clear();

            // find range of default FTFont
            int prevTo = -1;
            std::map<WORD, WORD>::iterator it_begin = final.begin();
            std::map<WORD, WORD>::iterator it_end = final.end();
            for (; it_begin != it_end; ++it_begin) {
                to = (it_begin->first) - 1;
                from = prevTo + 1;
                prevTo = it_begin->second;

                if (from <= to) {
                    info.defaultFTFontRange.push_back(FTFontRange::Create((WORD)from, (WORD)to));
                }
            }

            to = 0xffff;
            from = prevTo + 1;
            if (from <= to) {
                info.defaultFTFontRange.push_back(FTFontRange::Create((WORD)from, (WORD)to));
            }
            final.clear();
            fontNameInfo.insert(FTFontNameMap::value_type(fontGroupName, info));
        }
    }

    return true;
}

FTFontResources::FTFontResources()
{
    FT_Error error = FT_Init_FreeType(&_ftLib);
    if (error)
        Throw(::Exceptions::BasicLabel("Freetype font library failed to initialize (error code: %i)", error));

    LoadFontConfigFile();
    _fontTexMgr = std::make_unique<FT_FontTextureMgr>();
}

FTFontResources::~FTFontResources()
{
    FT_Done_FreeType(_ftLib);
}


/*void CleanupFTFontSystem()
{
	fontGroupMap.clear();
	damageDisplayFontGroupMap.clear();
	fontMap.clear();

    fontTexMgr = nullptr;
    damageDisplayFontTexMgr = nullptr;
    fontFileBufferManager = nullptr;

    if (ftLib) {
        FT_Done_FreeType(ftLib);
		ftLib = 0;
    }
}*/

}

