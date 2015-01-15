// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FontPrimitives.h"
#include <vector>
#include <memory>

namespace RenderOverlays
{

struct FontChar;

class FTFont;
class CharSlotArray;
class TextureHeap;

class FontTexture2D;

class FT_FontTextureMgr
{
public:
    bool            Init(int texWidth, int texHeight);
    void            CheckTextureValidate(FT_Face face, int size, FontChar *fc);

    bool            IsNeedReset() const;
    void            RequestReset();
    void            Reset();

    typedef std::vector<std::unique_ptr<CharSlotArray>> CharSlotArrayList;
    struct FontCharTable
    {
        std::vector<std::vector<std::pair<ucs4, FontCharID> > >  _table;
        FontCharID&         operator[](ucs4 ch);
        void                ClearTable();
        FontCharTable();
        ~FontCharTable();
    };

    class FontFace
    {
    public:
        const FontChar*     GetChar(int ch, FontTexKind kind);
        FontCharID          CreateChar(int ch, FontTexKind kind);
        void                DeleteChar(FontCharID fc);
        const FontTexture2D*    GetTexture() const { return _texture; }

        FontFace(TextureHeap* texHeap, FontTexture2D* texture, int texWidth, int texHeight);
    
        CharSlotArrayList   _slotList;
        FT_Face             _face;
        int                 _size;

    private:
        FontCharTable       _table;

        int                 _texWidth, _texHeight;
        TextureHeap *       _texHeap;
        FontTexture2D *     _texture;

        FontCharID          FindEmptyCharSlot(const FontChar& fc, int *x, int *y, FontTexKind kind);
    };

    FontFace*       FindFontFace(FT_Face face, int size);
    FontFace*       CreateFontFace(FT_Face face, int size);
    void            DeleteFontFace(FTFont* font);

    FT_FontTextureMgr();
    virtual ~FT_FontTextureMgr();

private:
    typedef std::vector<std::unique_ptr<FontFace>> FontFaceList;

    CharSlotArray*      OverwriteLRUChar(FontFace* face, FontTexKind kind);
    void                ClearFontTextureRegion(int height, int heightEnd);

    int                             _texWidth, _texHeight;
    FontFaceList                    _faceList;
    std::unique_ptr<TextureHeap>    _texHeap;
    std::unique_ptr<FontTexture2D>  _texture;
    bool                            _needReset;
};

}

