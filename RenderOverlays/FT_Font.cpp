// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_Font.h"
#include "FT_FontTexture.h"
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

// #ifdef WIN32
// #if _MSC_VER == 1600
//     #ifdef _DEBUG
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64d_vc100.lib" )
//         #else
//         #pragma comment( lib, "freetypex86d_vc100.lib" )
//         #endif
//     #else
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64_vc100.lib" )
//         #else
//         #pragma comment( lib, "freetypex86_vc100.lib" )
//         #endif
//     #endif
// #elif _MSC_VER == 1500
//     #ifdef _DEBUG
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64d_vc90.lib" )
//         #else
//         #pragma comment( lib, "freetypex86d_vc90.lib" )
//         #endif
//     #else
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64_vc90.lib" )
//         #else
//         #pragma comment( lib, "freetypex86_vc90.lib" )
//         #endif
//     #endif
// #else
//     #ifdef _DEBUG
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64d.lib" )
//         #else
//         #pragma comment( lib, "freetypex86d.lib" )
//         #endif
//     #else
//         #if CPU_X64
//         #pragma comment( lib, "freetypex64.lib" )
//         #else
//         #pragma comment( lib, "freetypex86.lib" )
//         #endif
//     #endif
// #endif
// #endif

struct FontDefLessPred {
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
class FontFileBufferManager {
public:
    FontFileBufferManager() {}
    ~FontFileBufferManager() 
    { 
        Destroy(); 
    }

    void Destroy()
    {
        for (auto it = _buffers.begin(); it != _buffers.end(); ++it) {
            assert(it->second->_refCount == 0);
        }

        _buffers.clear();
    }

    int GetCount() const { return (int)_buffers.size(); }

    uint8* GetBuffer(const char path[], size_t* bufferSize)
    {
        FontBufferMap::iterator it = _buffers.find(path);
        if (it != _buffers.end()) {
            ++it->second->_refCount;
            *bufferSize = it->second->_bufferSize;
            return it->second->_buffer.get();
        }

        std::unique_ptr<FontFileBuffer> buffer = CreateBuffer(path);
        if (!buffer) {
            return 0;
        }

        ++buffer->_refCount;
        *bufferSize = buffer->_bufferSize;
        auto i = _buffers.insert(std::make_pair(path, std::move(buffer)));
        return i.first->second->_buffer.get();
    }

    void ReleaseBuffer(const char* path)
    {
        FontBufferMap::iterator it = _buffers.find(path);
        if (it != _buffers.end()) {
            assert(it->second->_refCount > 0);
            --it->second->_refCount;
            if (it->second->_refCount == 0) {
                it->second.reset();
                _buffers.erase(it);
            }
        }
    }

private:
    struct FontFileBuffer {
        int _refCount;
        std::unique_ptr<uint8[]> _buffer;
        size_t _bufferSize;

        FontFileBuffer()
        {
            _refCount = 0;
            _bufferSize = 0;
        }

        ~FontFileBuffer() {}
    };

    std::unique_ptr<FontFileBuffer> CreateBuffer(const char path[])
    {
        std::unique_ptr<uint8[]> memBuffer;
        size_t nSize;
        {
            BasicFile fp(path, "rb");

            fp.Seek(0, SEEK_END);
            nSize = fp.TellP();
            fp.Seek(0, SEEK_SET);

            if (!nSize) {
                return nullptr;
            }

            memBuffer.reset(new uint8[nSize]);
            if (!fp.Read(memBuffer.get(), nSize, 1)) {
                return nullptr;
            }
        }

        std::unique_ptr<FontFileBuffer> buffer = std::make_unique<FontFileBuffer>();
        buffer->_buffer = std::move(memBuffer);
        buffer->_bufferSize = nSize;
        buffer->_refCount = 0;
        return buffer;
    }

    typedef std::map<std::string, std::unique_ptr<FontFileBuffer>> FontBufferMap;
    FontBufferMap _buffers;
};


typedef std::map<FontDef, FTFontGroup*, FontDefLessPred> UiFontGroupMap;     // awkwardly, these can't use smart ptrs... because these objects are removed from the maps in their destructors
typedef std::map<FontDef, FTFont*, FontDefLessPred> UiFontMap;

static UiFontGroupMap   fontGroupMap;
static UiFontGroupMap   damageDisplayFontGroupMap;
static UiFontMap        fontMap;

FT_Library ftLib = 0;

static std::unique_ptr<FT_FontTextureMgr>       fontTexMgr = NULL;
static std::unique_ptr<FT_FontTextureMgr>       damageDisplayFontTexMgr = NULL;
static std::unique_ptr<FontFileBufferManager>   fontFileBufferManager = NULL;

static FT_FontTextureMgr::FontFace* GetFontFace(FT_Face face, int size, FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY: 
        if(damageDisplayFontTexMgr) {
            return damageDisplayFontTexMgr->FindFontFace(face, size);
        }
        break;

    case FTK_GENERAL:
    default:
        if(fontTexMgr) {
            return fontTexMgr->FindFontFace(face, size);
        }
        break;
    }
    return NULL;
}

static void NotifyDeletingFont(FTFont* font, FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY: 
        if(damageDisplayFontTexMgr) {
            damageDisplayFontTexMgr->DeleteFontFace(font);
        }
        break;

    case FTK_GENERAL:
    default:
        if(fontTexMgr) {
            fontTexMgr->DeleteFontFace(font);
        }
        break;
    }
}

static void CheckTextureValidate(FT_Face face, int size, FontChar *fc, FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY: 
        if(damageDisplayFontTexMgr) {
            damageDisplayFontTexMgr->CheckTextureValidate(face, size, fc);
        }
        break;

    case FTK_GENERAL:
    default:
        if(fontTexMgr) {
            fontTexMgr->CheckTextureValidate(face, size, fc);
        }
    }
}

static void Register(const FontDef& def, FTFont* font)
{
    fontMap.insert(UiFontMap::value_type(def, font));
}

static void Deregister(FTFont* font)
{
    for (UiFontMap::const_iterator i=fontMap.cbegin(); i!=fontMap.cend(); ++i) {
        if (i->second == font) {
            fontMap.erase(i);
            return;
        }
    }
}

static void Register(const FontDef& def, FTFontGroup* fontGroup)
{
    if (fontGroup->GetTexKind() == FTK_DAMAGEDISPLAY) {
        damageDisplayFontGroupMap.insert(UiFontGroupMap::value_type(def, fontGroup));
    } else {
        fontGroupMap.insert(UiFontGroupMap::value_type(def, fontGroup));
    }
}

static void Deregister(FTFontGroup* fontGroup)
{
    for (UiFontGroupMap::const_iterator i=fontGroupMap.cbegin(); i!=fontGroupMap.cend(); ++i) {
        if (i->second == fontGroup) {
            fontGroupMap.erase(i);
            return;
        }
    }
    for (UiFontGroupMap::const_iterator i=damageDisplayFontGroupMap.cbegin(); i!=damageDisplayFontGroupMap.cend(); ++i) {
        if (i->second == fontGroup) {
            damageDisplayFontGroupMap.erase(i);
            return;
        }
    }
}

FTFont::FTFont(FontTexKind kind)
{
    _ascend = 0;
    _face = 0;
    _pBuffer = 0;
    _texKind = kind;
}

FTFont::~FTFont()
{
    NotifyDeletingFont(this, _texKind);
    Deregister(this);

    if (_face) {
        FT_Done_Face(_face);
        _face = 0;
    }

    if (_pBuffer) {
        fontFileBufferManager->ReleaseBuffer(_path);
    }
}

bool FTFont::Init(const FontDef& fontDef)
{
    Deregister(this);

    FT_Error error;

    XlCopyString(_path, fontDef.path.c_str());

    _size = fontDef.size;

    size_t bufferSize;

    if (!fontFileBufferManager) {
        // GameWarningF("no font file buffer manager");
        return false;
    }

    _pBuffer = fontFileBufferManager->GetBuffer(_path, &bufferSize);

    if (!_pBuffer) {
        // GameWarningF("failed to load font(%s)", path);
        return false;
    }


    FT_New_Memory_Face(ftLib, _pBuffer, (FT_Long)bufferSize, 0, &_face);

    error = FT_Set_Pixel_Sizes(_face, 0, _size);
    if (error) {
        // GameWarning("Failed to set pixel size");
    }

    error = FT_Load_Char(_face, ' ', FT_LOAD_RENDER);
    if (error) {
        // GameWarning("There is no blank character(%s)", _path);
    }

    error = FT_Load_Char(_face, 'X', FT_LOAD_RENDER);
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

    Register(fontDef, this);

    return true;
}

FontCharID FTFont::CreateFontChar(ucs4 ch) const
{
    switch (_texKind) {
    case FTK_DAMAGEDISPLAY: 
        if(damageDisplayFontTexMgr) {
            FT_FontTextureMgr::FontFace* face = damageDisplayFontTexMgr->FindFontFace(_face, _size);
            if (!face) {
                face = damageDisplayFontTexMgr->CreateFontFace(_face, _size);
            }
            return face->CreateChar(ch, _texKind);
        }
        break;

    case FTK_GENERAL:
    default:
        if(fontTexMgr) {
            FT_FontTextureMgr::FontFace* face = fontTexMgr->FindFontFace(_face, _size);
            if (!face) {
                face = fontTexMgr->CreateFontFace(_face, _size);
            }
            return face->CreateChar(ch, _texKind);
        }
        break;
    }
    return FontCharID_Invalid;
}

void FTFont::DeleteFontChar(FontCharID fc)
{
    FT_FontTextureMgr::FontFace* fontFace = GetFontFace(_face, _size, _texKind);
    if (fontFace) {
        fontFace->DeleteChar(fc);
    }
}

std::pair<const FontChar*, const FontTexture2D*> FTFont::GetChar(ucs4 ch) const
{
    switch (_texKind) {
    case FTK_DAMAGEDISPLAY: 
        if(damageDisplayFontTexMgr) {
            FT_FontTextureMgr::FontFace* face = damageDisplayFontTexMgr->FindFontFace(_face, _size);
            if (!face) {
                face = damageDisplayFontTexMgr->CreateFontFace(_face, _size);
            }
            return std::make_pair(face->GetChar(ch, _texKind), face->GetTexture());
        }
        break;

    case FTK_GENERAL:
    default:
        if(fontTexMgr) {
            FT_FontTextureMgr::FontFace* face = fontTexMgr->FindFontFace(_face, _size);
            if (!face) {
                face = fontTexMgr->CreateFontFace(_face, _size);
            }
            return std::make_pair(face->GetChar(ch, _texKind), face->GetTexture());
        }
        break;
    }

    return std::pair<const FontChar*, const FontTexture2D*>(nullptr, nullptr);
}

void FTFont::TouchFontChar(const FontChar *fc)
{
    // fc->usedTime = desktop.time;
    CheckTextureValidate(_face, _size, const_cast<FontChar*>(fc), _texKind);
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
    int currentGlyph = FT_Get_Char_Index(_face, ch); 
    if(*curGlyph)
        *curGlyph = currentGlyph;

    if (prevGlyph) {
        FT_Vector kerning;
        FT_Get_Kerning(_face, prevGlyph, currentGlyph, FT_KERNING_DEFAULT, &kerning);

        return Float2((float)kerning.x / 64, (float)kerning.y / 64);
    }

    return Float2(0.0f, 0.0f);
}

float FTFont::GetKerning(ucs4 prev, ucs4 ch) const
{
    if (prev) {
        int prevGlyph = FT_Get_Char_Index(_face, prev);
        int curGlyph = FT_Get_Char_Index(_face, ch); 

        FT_Vector kerning;
        FT_Get_Kerning(_face, prevGlyph, curGlyph, FT_KERNING_DEFAULT, &kerning);
        return (float)kerning.x / 64;
    }

    return 0.0f;
}



static FTFontNameMap fontNameInfo;

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
    Deregister(this);
}

bool FTFontGroup::CheckMyFace(FT_Face face)
{
    if (_defaultFTFont && _defaultFTFont->GetFace() == face) {
        return true;
    } else {
        SubFTFontInfoMap::iterator it_begin = _subFTFontInfoMap.begin();
        SubFTFontInfoMap::iterator it_end = _subFTFontInfoMap.end();
        FTFont* subFTFont = NULL;
        for (; it_begin != it_end; ++it_begin) {
            subFTFont = (FTFont*)(it_begin->first);
            if (subFTFont && subFTFont->GetFace() == face)
                return true;
        }
    }

    return false;
}

intrusive_ptr<FTFont> FTFontGroup::FindFTFontByChar(ucs4 ch) const
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
        FTFont* subFTFont = it_begin->first;
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
        _defaultFTFont = make_intrusive<FTFont>(_texKind);

        if (!_defaultFTFont->Init(fd)) {
            _defaultFTFont.reset();
            // GameWarning("Failed to create freetype font '%s'", fd.path.c_str());
            return false;
        }
    }

    if (_defaultFTFont) {
        _defaultFTFontRanges = info.defaultFTFontRange;
    }

    return true;
}

void FTFontGroup::LoadSubFTFont(FTFontNameInfo &info, int size)
{
    FontDef fd;
    intrusive_ptr<FTFont> font;
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
            font = make_intrusive<FTFont>(_texKind);
            if (!font->Init(fd)) {
                font = nullptr;
                // GameWarning("Failed to create freetype font '%s'", fd.path.c_str());
            }
        }
        
        if (font) {
            _subFTFontInfoMap.insert(SubFTFontInfoMap::value_type(font.get(), (FTFontRanges)it_begin->second));
        }
    }
}

std::pair<const FontChar*, const FontTexture2D*> FTFontGroup::GetChar(ucs4 ch) const
{
    intrusive_ptr<FTFont> font = FindFTFontByChar(ch);
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
    intrusive_ptr<FTFont> font = FindFTFontByChar(ch);
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
    intrusive_ptr<FTFont> font = FindFTFontByChar(ch);
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
            Register(def, this);
            return true;
        }
    } else {
        FTFontNameInfo info;
        info.defaultFTFontPath = _path;
        info.defaultFTFontRange.push_back(FTFontRange::Create(0, 0xffff));
        fontNameInfo.insert(FTFontNameMap::value_type(_path, info));
        if (LoadDefaultFTFont(info, _size)) {
            LoadSubFTFont(info, _size);
            Register(def, this);
            return true;
        }
    }

    return false;
}

FontCharID FTFontGroup::CreateFontChar(ucs4 ch)
{
    intrusive_ptr<FTFont> font = FindFTFontByChar(ch);
    if (font) {
        return font->CreateFontChar(ch);
    }

    return FontCharID(0);
}

float FTFontGroup::GetKerning(ucs4 prev, ucs4 ch) const
{
    intrusive_ptr<FTFont> font = FindFTFontByChar(ch);
    if (font) {
        return font->GetKerning(prev, ch);
    }

    return 0.0f;
}

    // DavidJ -- hack! subvert massive virtual call overload by putting in some quick overloads
intrusive_ptr<const Font> FTFontGroup::GetSubFont(ucs4 ch) const 
{
    return FindFTFontByChar(ch);      // DavidJ -- warning -- lots of redundant AddRef/Releases involved with this call
}

bool FTFontGroup::IsMultiFontAdapter() const { return true; }

static intrusive_ptr<FTFontGroup> LoadFTFont(const FontDef& def, FontTexKind kind)
{
    intrusive_ptr<FTFontGroup> fontGroup = make_intrusive<FTFontGroup>(kind);
    if (!fontGroup->Init(def)) {
        fontGroup = nullptr;
        // GameWarning("Failed to create freetype font '%s'", path);
    }

    return fontGroup;
}

intrusive_ptr<FTFont> GetX2FTFont(const char* path, int size, FontTexKind kind)
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

    return LoadFTFont(fd, kind);
}

bool InitFTFontSystem()
{
    FT_Error error = FT_Init_FreeType(&ftLib);
    if (error) {
        return false;
    }

    LoadFontConfigFile();

    if (!fontFileBufferManager) {
        fontFileBufferManager = std::make_unique<FontFileBufferManager>();
    }

    if (!fontTexMgr) {
        fontTexMgr = std::make_unique<FT_FontTextureMgr>();
        fontTexMgr->Init(1024, 1024);
    }

    // if (!damageDisplayFontTexMgr) {
    //     damageDisplayFontTexMgr = std::make_unique<FT_FontTextureMgr>();
    //     damageDisplayFontTexMgr->Init(1024, 2048);
    // }

    // if (desktop.renderer && !desktop.renderer->IsDeviceLost()) {
    //     devideResetFrame = desktop.renderer->GetFrameReset();
    // } else {
    //     devideResetFrame = 0;
    // }

    return true;
}

static bool LoadDataFromPak(const char* path, Data* out)
{
    std::unique_ptr<char[]> str;
    size_t read, size = 0;

    {
        BasicFile file(path, "rb");

        file.Seek(0, SEEK_END);
        size = file.TellP();
        file.Seek(0, SEEK_SET);
        if (!size) {
            return false;
        }
    
        str.reset(new char[size]);
        read = (size_t)file.Read(str.get(), 1, size);
    }

    bool result;
    if (read == size) {
        result = out->Load(str.get(), (int)size);
    } else {
        result = false;
    }
    return result;
}

bool LoadFontConfigFile()
{
    Data config;    
    if (!LoadDataFromPak("game/fonts/fonts.g", &config)) {
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

void CleanupFTFontSystem()
{
    assert(fontGroupMap.empty());
    assert(damageDisplayFontGroupMap.empty());
    assert(fontMap.empty());

    fontTexMgr = nullptr;
    damageDisplayFontTexMgr = nullptr;
    fontFileBufferManager = nullptr;

    if (ftLib) {
        FT_Done_FreeType(ftLib);
		ftLib = 0;
    }
}

void CheckResetFTFontSystem()
{
//     if (desktop.renderer) {
//         if (!desktop.renderer->IsDeviceLost()) {
//             if (devideResetFrame != desktop.renderer->GetFrameReset()) {
//                 devideResetFrame = desktop.renderer->GetFrameReset();
// 
//                 if (fontTexMgr) {
//                     fontTexMgr->RequestReset();
//                 }
// 
//                 if (damageDisplayFontTexMgr) {
//                     damageDisplayFontTexMgr->RequestReset();
//                 }
//             }
//         }
//     }

    if (fontTexMgr && fontTexMgr->IsNeedReset()) {
        // for (UiFontGroupMap::iterator it=fontGroupMap.begin(); it!=fontGroupMap.end(); ++it) {
        //     it->second->ResetTable();
        // }

        fontTexMgr->Reset();
    }

    if (damageDisplayFontTexMgr && damageDisplayFontTexMgr->IsNeedReset()) {
        // for (UiFontGroupMap::iterator it=damageDisplayFontGroupMap.begin(); it!=damageDisplayFontGroupMap.end(); ++it) {
        //     it->second->ResetTable();
        // }

        damageDisplayFontTexMgr->Reset();
    }
}

int GetFTFontCount(FontTexKind kind)
{
    switch (kind) {
    case FTK_DAMAGEDISPLAY:
        {
            UiFontGroupMap::iterator it_begin = damageDisplayFontGroupMap.begin();
            UiFontGroupMap::iterator it_end = damageDisplayFontGroupMap.end();
            FTFontGroup* fontGroup = NULL;
            int count = 0;
            for (; it_begin != it_end; ++it_begin) {
                fontGroup = (FTFontGroup*)(it_begin->second);
                if (fontGroup) {
                    count += fontGroup->GetFTFontCount();
                }
            }
            return count;
        }        

    case FTK_GENERAL:
        {
            UiFontGroupMap::iterator it_begin = fontGroupMap.begin();
            UiFontGroupMap::iterator it_end = fontGroupMap.end();
            FTFontGroup* fontGroup = NULL;
            int count = 0;
            for (; it_begin != it_end; ++it_begin) {
                fontGroup = (FTFontGroup*)(it_begin->second);
                if (fontGroup) {
                    count += fontGroup->GetFTFontCount();
                }
            }
            return count;
        }
    }

    return 0;
}

int GetFTFontFileCount()
{
    if (!fontFileBufferManager) {
        return 0;
    }

    return fontFileBufferManager->GetCount();
}

}

