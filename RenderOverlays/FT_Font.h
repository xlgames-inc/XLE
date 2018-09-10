// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FT_FontTexture.h"
#include "Font.h"
#include "FontPrimitives.h"
#include "../Assets/IFileSystem.h"
#include "../Utility/StringUtils.h"
#include <map>
#include <vector>
#include <memory>

typedef struct FT_FaceRec_ FT_FaceRec;
typedef struct FT_FaceRec_* FT_Face;

namespace RenderOverlays
{

struct FTFontRange 
{
    uint16 from;
    uint16 to;

    static FTFontRange Create(uint16 f, uint16 t)
    {
        FTFontRange range;
        range.from = f;
        range.to = t;

        return range;
    }
};

class FTFont;
class FTFontGroup;

typedef std::vector<FTFontRange> FTFontRanges;
typedef std::map<std::shared_ptr<FTFont>, FTFontRanges> SubFTFontInfoMap;

struct FontDef 
{
    std::string path;
    int size;
};

// freetype font
class FTFont : public Font 
{
public:
    FTFont();
    virtual ~FTFont();

    //std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const;

    virtual FT_Face GetFace() { return _face.get(); }
    virtual FT_Face GetFace(ucs4 /*ch*/) { return _face.get(); }

    virtual bool Init(const FontDef& fontDef);
    virtual float Descent() const;
    virtual float Ascent(bool includeAccent) const;
    virtual float LineHeight() const;
    // virtual bool SacrificeChar(int ch);
    // virtual void TouchFontChar(const FontChar *fc);
    virtual Float2 GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const;

protected:
    virtual FontGlyphID  CreateFontChar(ucs4 ch) const;
    virtual void        DeleteFontChar(FontGlyphID fc);
    virtual float       GetKerning(ucs4 prev, ucs4 ch) const;

    int _ascend;
    std::shared_ptr<FT_FaceRec_> _face;
    ::Assets::Blob _pBuffer;

	std::shared_ptr<FT_FontTextureMgr::FontFace> _textureFace;

    friend class FTFontGroup;
};

typedef std::map<std::string, FTFontRanges> FTFontInfoMap;

struct FTFontNameInfo
{
    std::string defaultFTFontPath;
    FTFontRanges defaultFTFontRange;
    FTFontInfoMap subFTFontInfo;
};

typedef std::map<std::string, FTFontNameInfo> FTFontNameMap;

#if 0
class FTFontGroup : public FTFont
{
public:
    FTFontGroup();
    virtual ~FTFontGroup();

    bool CheckMyFace(FT_Face face);
    std::shared_ptr<FTFont> FindFTFontByChar(ucs4 ch) const;
    int GetFTFontCount();
    bool LoadDefaultFTFont(FTFontNameInfo &info, int size);
    void LoadSubFTFont(FTFontNameInfo &info, int size);

    // virtual std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const;

    virtual FT_Face GetFace();
    virtual FT_Face GetFace(ucs4 ch);

    virtual float Descent();
    virtual float Ascent(bool includeAccent) const;
    virtual float LineHeight() const;
    virtual Float2 GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const;

    virtual bool Init(const FontDef& def);
    // virtual bool SacrificeChar(int ch);

    virtual std::shared_ptr<const Font> GetSubFont(ucs4 ch) const;
    virtual bool IsMultiFontAdapter() const;

private:
    virtual FontGlyphID CreateFontChar(ucs4 ch);
    virtual float GetKerning(ucs4 prev, ucs4 ch) const;

	std::shared_ptr<FTFont> _defaultFTFont;
    FTFontRanges _defaultFTFontRanges;
    SubFTFontInfoMap _subFTFontInfoMap;

    // friend void GarbageCollectFTFontSystem();
    // friend void CheckResetFTFontSystem();
};

bool    InitFTFontSystem();
bool    LoadFontConfigFile();
void    CleanupFTFontSystem();
#endif

// void    CheckResetFTFontSystem();
// int     GetFTFontCount(FontTexKind kind);
// int     GetFTFontFileCount();

std::shared_ptr<FTFont> GetX2FTFont(StringSection<> path, int size);

}

