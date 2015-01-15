// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Font.h"
#include "FontPrimitives.h"
#include "../Utility/StringUtils.h"
#include <map>
#include <vector>

namespace RenderOverlays
{

struct FTFontRange {
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
typedef std::map<FTFont*, FTFontRanges> SubFTFontInfoMap;       // (DavidJ -- can't use intrusive_ptr as the first member of a map)

struct FontDef {
    std::string path;
    int size;
};

// freetype font
class FTFont : public Font {
public:
    FTFont(FontTexKind kind = FTK_GENERAL);
    virtual ~FTFont();

    std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const;
    FontTexKind GetTexKind() { return _texKind; }

    virtual FT_Face GetFace() { return _face; }
    virtual FT_Face GetFace(ucs4 /*ch*/) { return _face; }

    virtual bool Init(const FontDef& fontDef);
    virtual float Descent() const;
    virtual float Ascent(bool includeAccent) const;
    virtual float LineHeight() const;
    // virtual bool SacrificeChar(int ch);
    virtual void TouchFontChar(const FontChar *fc);
    virtual Float2 GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const;

protected:
    virtual FontCharID  CreateFontChar(ucs4 ch) const;
    virtual void        DeleteFontChar(FontCharID fc);
    virtual float       GetKerning(ucs4 prev, ucs4 ch) const;

    int _ascend;
    FT_Face _face;
    unsigned char* _pBuffer;

    FontTexKind _texKind;

    friend class FTFontGroup;
};

typedef std::map<std::string, FTFontRanges> FTFontInfoMap;

struct FTFontNameInfo {
    std::string defaultFTFontPath;
    FTFontRanges defaultFTFontRange;
    FTFontInfoMap subFTFontInfo;
};
typedef std::map<std::string, FTFontNameInfo> FTFontNameMap;

class FTFontGroup : public FTFont {
public:
    FTFontGroup(FontTexKind kind = FTK_GENERAL);
    virtual ~FTFontGroup();

    bool CheckMyFace(FT_Face face);
    intrusive_ptr<FTFont> FindFTFontByChar(ucs4 ch) const;
    int GetFTFontCount();
    bool LoadDefaultFTFont(FTFontNameInfo &info, int size);
    void LoadSubFTFont(FTFontNameInfo &info, int size);

    virtual std::pair<const FontChar*, const FontTexture2D*> GetChar(ucs4 ch) const;

    virtual FT_Face GetFace();
    virtual FT_Face GetFace(ucs4 ch);

    virtual float Descent();
    virtual float Ascent(bool includeAccent) const;
    virtual float LineHeight() const;
    virtual Float2 GetKerning(int prevGlyph, ucs4 ch, int* curGlyph) const;

    virtual bool Init(const FontDef& def);
    // virtual bool SacrificeChar(int ch);

    virtual intrusive_ptr<const Font> GetSubFont(ucs4 ch) const;
    virtual bool IsMultiFontAdapter() const;

private:
    virtual FontCharID CreateFontChar(ucs4 ch);
    virtual float GetKerning(ucs4 prev, ucs4 ch) const;

    intrusive_ptr<FTFont> _defaultFTFont;
    FTFontRanges _defaultFTFontRanges;
    SubFTFontInfoMap _subFTFontInfoMap;

    friend void GarbageCollectFTFontSystem();
    friend void CheckResetFTFontSystem();
};

bool    InitFTFontSystem();
bool    LoadFontConfigFile();
void    CleanupFTFontSystem();
void    CheckResetFTFontSystem();
int     GetFTFontCount(FontTexKind kind);
int     GetFTFontFileCount();

intrusive_ptr<FTFont> GetX2FTFont(const char* path, int size, FontTexKind kind = FTK_GENERAL);

}

