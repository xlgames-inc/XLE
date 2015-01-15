// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FT_Font.h"
#include "FT_FontTexture.h"
#include "FontRendering.h"
#include "../Core/Types.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../RenderCore/Metal/Format.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"

#include <assert.h>
#include <algorithm>
#include <functional>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace RenderOverlays
{

#pragma warning(disable:4127)

class CharSlotArray
{
private:
    int         _height;
    int         _heightEnd;
    uint32      _usedCount;
    std::vector<FontChar> _array;

public:
    CharSlotArray(int height, int heightEnd, int count);
    ~CharSlotArray();

public:
    bool            IsFull() const;
    inline int      GetSlotHeight() const { return _height; }
    inline int      GetSlotHeightEnd() const { return _heightEnd; }

public:
        //if there is not empty slot, return -1
    FontCharID      UseEmptySlot(const FontChar& fc);
    bool            ReleaseSlot(FontCharID id);
    const FontChar* GetLeastUsedChar() const;
    const FontChar* GetChar(unsigned index) const;
    FontChar*       GetChar(unsigned index);
};

CharSlotArray::CharSlotArray(int height, int heightEnd, int count)
{
    _height = height;
    _heightEnd = heightEnd;
    _usedCount = 0;

    _array.resize(count);
}

CharSlotArray::~CharSlotArray()
{

}

bool CharSlotArray::IsFull() const
{
    if (_usedCount >= _array.size()) {
        return true;
    }

    return false;
}

static bool IsUsed(const FontChar& fc) { return fc.ch != 0; }

FontCharID CharSlotArray::UseEmptySlot(const FontChar& fc)
{
    uint32 maxCount = (uint32)_array.size();
    for (uint32 i = 0; i < maxCount; i++) {
        if (!IsUsed(_array[i])) {
            _array[i] = fc;
            _usedCount++;
            return FontCharID(i);
        }
    }

    //cannot be reached
    return FontCharID_Invalid;
}

bool CharSlotArray::ReleaseSlot(FontCharID id)
{
    if (id < _array.size()) {
        _array[id].ch = 0;
        return true;
    }
    return false;
}

const FontChar* CharSlotArray::GetLeastUsedChar() const
{
    const FontChar* fontCh = &_array[0];
    uint32 maxCount = (uint32)_array.size();
    for(uint32 i = 1; i < maxCount; i++) {
        const FontChar& cursor = _array[i];
        if (IsUsed(*fontCh) && IsUsed(cursor)) {
            if (fontCh->usedTime > cursor.usedTime) {
                fontCh = &cursor;
            }
        } else if (!IsUsed(*fontCh)) {       // (DavidJ -- is this correctly? Or was it intended to be 
            fontCh = &_array[i];
        }
    }
    return fontCh;
}

const FontChar* CharSlotArray::GetChar(unsigned index) const
{
    if (index < _array.size()) {
        return &_array[index];
    }
    return NULL;
}

FontChar* CharSlotArray::GetChar(unsigned index)
{
    if (index < _array.size()) {
        return &_array[index];
    }
    return NULL;
}

//-------------------------------------------------------------------------------------------------

class TextureHeap
{
private:
    enum { CHUNK_MARGIN = 1 };

    struct Chunk
	{
		int _start;
		int _end;
		Chunk *_prev;
		Chunk *_next;
		bool _used;

        Chunk(int start, int end)
        {
            _start = start;
	        _end   = end;
	        _prev  = NULL;
	        _next  = NULL;
	        _used  = false;
        }

		inline int AllocSize() const { return _end - _start; }
	};

	typedef std::vector<std::unique_ptr<Chunk>> UsedList;
	UsedList _usedList;

	typedef std::vector<std::unique_ptr<Chunk>> EmptyList;
	EmptyList _emptyList;

    int _heapSize;

public:
    bool IsAllocable(int size);
    std::unique_ptr<CharSlotArray> CreateCharSlotArray(int size, int maxCount);
    void DestroyCharSlotArray(CharSlotArray* slot);

private:
    bool AllocNewChunk(int size, int& startPos, int& endPos);
    void DeAllocChunk(int startPos);

    Chunk* FindBestChunk(int size);
    Chunk* FindUsedChunk(int startPos);

public:
    TextureHeap(int size);
    virtual ~TextureHeap();
};

TextureHeap::TextureHeap(int size)
{
    _heapSize = size;
	_emptyList.push_back(std::make_unique<Chunk>(0, _heapSize));
}

TextureHeap::~TextureHeap()
{
    if( !_usedList.empty() ) {
		// GameWarning("TextureHeap - %ld chunks not released", _usedList.size());
		_usedList.clear();
	}

	// if( _emptyList.size() != 1 ) {
	// 	GameWarning("TextureHeap - too many empty chunks(%ld) remained", _emptyList.size() );
	// }
	_emptyList.clear();
}

bool TextureHeap::IsAllocable(int size)
{
    if( FindBestChunk(size + CHUNK_MARGIN))
        return true;

    return false;
}

std::unique_ptr<CharSlotArray> TextureHeap::CreateCharSlotArray(int size, int maxCount)
{
    int startPos, endPos;
    AllocNewChunk(size + CHUNK_MARGIN, startPos, endPos);
    return std::make_unique<CharSlotArray>(startPos, endPos, maxCount);
}

void TextureHeap::DestroyCharSlotArray(CharSlotArray* slot)
{
    if(slot) {
        DeAllocChunk(slot->GetSlotHeight());
        // delete slot;     (delete is called by the owner, now)
    }
}

bool TextureHeap::AllocNewChunk(int size, int& startPos, int& endPos)
{
    startPos = endPos = 0;

    Chunk *bestChunk = FindBestChunk(size);
    if(!bestChunk) {
        // GameWarning("TextureHeap - overflow");
        return false;
    }

    std::unique_ptr<Chunk> usedChunk;
    if(bestChunk->AllocSize() == size) {
        EmptyList::iterator i = std::remove_if(_emptyList.begin(), _emptyList.end(), [=](const std::unique_ptr<Chunk>& i){return i.get() == bestChunk;});
        std::swap(usedChunk, *i);
        _emptyList.erase(i);
    } else {
        usedChunk = std::make_unique<Chunk>(bestChunk->_start, bestChunk->_start + size);

		bestChunk->_start = usedChunk->_end;
		usedChunk->_prev = bestChunk->_prev;

        if( bestChunk->_prev )
			bestChunk->_prev->_next = usedChunk.get();

		usedChunk->_next = bestChunk;
		bestChunk->_prev = usedChunk.get();
    }

    usedChunk->_used = true;
	startPos = usedChunk->_start;
    endPos   = usedChunk->_end;
    _usedList.push_back(std::move(usedChunk));
	return true;
}

void TextureHeap::DeAllocChunk(int startPos)
{
	Chunk *usedChunk = FindUsedChunk(startPos);

    Chunk *prev = usedChunk->_prev;
	if( prev && !prev->_used ) {
		usedChunk->_start = prev->_start;
		usedChunk->_prev  = prev->_prev;

        if( prev->_prev )
			prev->_prev->_next = usedChunk;

        _emptyList.erase(std::remove_if(_emptyList.begin(), _emptyList.end(), [=](const std::unique_ptr<Chunk>& i){return i.get() == prev;}));
	}

	Chunk *next = usedChunk->_next;
	if( next && !next->_used ) {
		usedChunk->_end  = next->_end;
		usedChunk->_next = next->_next;
		
        if( next->_next )
			next->_next->_prev = usedChunk;

		_emptyList.erase(std::remove_if(_emptyList.begin(), _emptyList.end(), [=](const std::unique_ptr<Chunk>& i){return i.get() == next;}));
	}

    UsedList::iterator i = std::partition(_usedList.begin(), _usedList.end(), [=](const std::unique_ptr<Chunk>& i){return i.get() != usedChunk;});
    assert(*i);
    std::unique_ptr<Chunk> newPtr; std::swap(newPtr, *i);
    _usedList.erase(i);
	newPtr->_used = false;
	_emptyList.push_back(std::move(newPtr));
}

TextureHeap::Chunk* TextureHeap::FindBestChunk(int size)
{
    EmptyList::iterator iter = _emptyList.begin();
    for( ; iter != _emptyList.end(); ++iter) {
        if((*iter)->AllocSize() >= size)
            return iter->get();
    }

    return NULL;
}

TextureHeap::Chunk* TextureHeap::FindUsedChunk(int startPos)
{
    UsedList::iterator iter = _usedList.begin();
    for( ; iter != _usedList.end(); ++iter) {
        if((*iter)->_start == startPos)
            return iter->get();
    }

	// GameWarning("TextureHeap - cannot find used chunk(%d)", startPos);
	return NULL;
}

//------------------------------------------------------

FT_FontTextureMgr::FT_FontTextureMgr()
{
    _texWidth = 0;
    _texHeight = 0;

    // _tex = 0;

    _needReset = false;
}

FT_FontTextureMgr::~FT_FontTextureMgr()
{
    if(!_faceList.empty()) {
        FontFaceList::iterator faceIter = _faceList.begin();
        for( ; faceIter != _faceList.end(); ++faceIter) {
            std::unique_ptr<FontFace>& face = *faceIter;

            CharSlotArrayList::iterator slotIter = face->_slotList.begin();
            for( ; slotIter != face->_slotList.end(); ++slotIter) {
                _texHeap->DestroyCharSlotArray((*slotIter).get());
            }

            face.reset();
        }
        _faceList.clear();
    }

    _texHeap.reset();
}

static int NextPower2(int n)
{
    int result = 1;
    while (result < n) result <<= 1;
    return result;
}

extern BufferUploads::IManager* gBufferUploads;

FontTexture2D::FontTexture2D(unsigned width, unsigned height, unsigned pixelFormat)
{
    using namespace BufferUploads;
    BufferDesc desc;
    desc._type = BufferDesc::Type::Texture;
    desc._bindFlags = BindFlag::ShaderResource;
    desc._cpuAccess = CPUAccess::Write;
    desc._gpuAccess = GPUAccess::Read;
    desc._allocationRules = 0;
    desc._textureDesc = TextureDesc::Plain2D(width, height, pixelFormat, 1);
    XlCopyString(desc._name, "Font");
    _transaction = gBufferUploads->Transaction_Begin(
        desc, (BufferUploads::RawDataPacket*)nullptr, BufferUploads::TransactionOptions::ForceCreate|BufferUploads::TransactionOptions::LongTerm);
}

FontTexture2D::~FontTexture2D()
{
    if (_transaction != ~BufferUploads::TransactionID(0x0)) {
        gBufferUploads->Transaction_End(_transaction); 
        _transaction = ~BufferUploads::TransactionID(0x0);
    }
}

void FontTexture2D::UpdateGlyphToTexture(FT_GlyphSlot glyph, int offX, int offY, int width, int height)
{
    auto packet = BufferUploads::CreateBasicPacket(
        width*height, nullptr, std::make_pair(width, width*height));
    uint8* data = (uint8*)packet->GetData(0,0);

    int widthCursor = 0;
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                uint8 pixel = 0;
                if (i < int(glyph->bitmap.width) && j < int(glyph->bitmap.rows))
                    pixel = glyph->bitmap.buffer[i + glyph->bitmap.width * j];

                data[i + widthCursor] = pixel;
            }
        }
        widthCursor += width;
    }

    UpdateToTexture(packet.get(), offX, offY, width, height);
}

void FontTexture2D::UpdateToTexture(BufferUploads::RawDataPacket* packet, int offX, int offY, int width, int height)
{
    if (_transaction == ~BufferUploads::TransactionID(0x0)) {
        _transaction = gBufferUploads->Transaction_Begin(_locator);
    }

    gBufferUploads->UpdateData(_transaction, packet, BufferUploads::Box2D(offX, offY, offX+width, offY+height));
}

void* FontTexture2D::GetUnderlying() const
{
    if ((!_locator || _locator->IsEmpty()) && _transaction) {
        if (gBufferUploads->IsCompleted(_transaction)) {
            _locator = gBufferUploads->GetResource(_transaction);
            if (_locator && !_locator->IsEmpty()) {
                gBufferUploads->Transaction_End(_transaction);
                _transaction = ~BufferUploads::TransactionID(0x0);
            }
        }
    }
    return _locator?_locator->GetUnderlying():nullptr;
}

bool FT_FontTextureMgr::Init(int texWidth, int texHeight)
{
    _texWidth = NextPower2(texWidth);
    _texHeight = NextPower2(texHeight);
    _texture = std::make_unique<FontTexture2D>(_texWidth, _texHeight, RenderCore::Metal::NativeFormat::R8_UNORM);
    _texHeap = std::make_unique<TextureHeap>(_texHeight);
    return true;
}

void FT_FontTextureMgr::CheckTextureValidate(FT_Face face, int size, FontChar *fc)
{
    if(!fc->needTexUpdate /*|| fc->tex*/) {
        return;
    }

    FT_Error error = FT_Load_Char(face, fc->ch, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT);
    if (error)  return;

    FT_GlyphSlot glyph = face->glyph;

    int updateWidth = 0;
    int updateHeight = 0;

    if(0/*fc->tex == _tex*/) {
        updateWidth = face->size->metrics.max_advance / 64;
        updateHeight = size;
    } else {
        updateWidth = NextPower2(glyph->bitmap.width);
        updateHeight = NextPower2(glyph->bitmap.rows);
    }

    // UpdateGlyphToTexture(glyph, fc->tex->GetTextureID(), fc->offsetX, fc->offsetY, updateWidth, updateHeight);

    fc->needTexUpdate = false;
}

void FT_FontTextureMgr::RequestReset()
{
    _needReset = true;
}

bool FT_FontTextureMgr::IsNeedReset() const
{
    return _needReset;
}

void FT_FontTextureMgr::Reset()
{
    if(!IsNeedReset())  return;

    FontFaceList::iterator faceIter = _faceList.begin();
    for( ; faceIter != _faceList.end(); ++faceIter) {
        std::unique_ptr<FontFace>& face = *faceIter;

        CharSlotArrayList::iterator slotIter = face->_slotList.begin();
        for( ; slotIter != face->_slotList.end(); ++slotIter) {
            _texHeap->DestroyCharSlotArray((*slotIter).get());
        }
    }
    _faceList.clear();
    _needReset = false;
}

FontCharID FT_FontTextureMgr::FontFace::CreateChar(int ch, FontTexKind kind)
{
    // if (!_tex) {
    //     // GameWarning("FT_FontTextureMgr::CreateChar - not initialized");
    //     return 0;
    // }

    FT_Error error = FT_Load_Char(_face, ch, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT);
    if (error) {
        if(ch != ' ') {
            // GameWarning("Failed to load character \'%d\'", ch);

            // FontChar* fc = CreateEmptyFontChar(ch);
            // fc->xAdvance = size * 0.5f;
            // return fc;
            return FontCharID_Invalid;
        } else {
            error = FT_Load_Char(_face, ch, FT_LOAD_RENDER);
            if (error) {
                // FontChar* fc = CreateEmptyFontChar(ch);
                // fc->xAdvance = size * 0.5f;
                // return fc;
                return FontCharID_Invalid;
            }
        }
    }

    FT_GlyphSlot glyph = _face->glyph;

    FontChar fc(ch);
    fc.left     = (float)glyph->bitmap_left;
    fc.top      = (float)glyph->bitmap_top;
    fc.width    = (float)glyph->bitmap.width;
    fc.height   = (float)glyph->bitmap.rows;
    fc.xAdvance = (float)glyph->advance.x / 64.0f;

    // assert((face->size->metrics.max_advance / 64) < glyph->bitmap.width);

    int offsetX, offsetY;
    FontCharID charID = FindEmptyCharSlot(fc, &offsetX, &offsetY, kind);
    if (charID != FontCharID_Invalid) {
        if (_texture) {
            _texture->UpdateGlyphToTexture( glyph, offsetX, offsetY,
                                            _face->size->metrics.max_advance / 64, _size);
        }
        return charID;
    } else {
        return FontCharID_Invalid;
        // RequestReset();
        // 
        // int texWidth = NextPower2(glyph->bitmap.width);
        // int texHeight = NextPower2(glyph->bitmap.rows);
        // // ITexture *newTex = CreateTempTexture(glyph, texWidth, texHeight);
        // // fc->tex = newTex;
        // 
        // fc.u0 = 0.0f;
        // fc.v0 = 0.0f;
        // fc.u1 = (float)glyph->bitmap.width / texWidth;
        // fc.v1 = (float)glyph->bitmap.rows / texHeight;
        // 
        // fc.offsetX = 0;
        // fc.offsetY = 0;
    }
}

FT_FontTextureMgr::FontFace* FT_FontTextureMgr::FindFontFace(FT_Face face, int size)
{
    auto it = _faceList.begin();
    for( ; it != _faceList.end(); ++it) {
        if((*it)->_face == face && (*it)->_size == size)
            return (*it).get();
    }

    return NULL;
}

auto FT_FontTextureMgr::CreateFontFace(FT_Face face, int size) -> FontFace*
{
    if(!_texHeap->IsAllocable(size)) {
        return nullptr;
    } else {
        {
            std::unique_ptr<FontFace> fontFace = std::make_unique<FontFace>(_texHeap.get(), _texture.get(), _texWidth, _texHeight);
            fontFace->_face = face;
            fontFace->_size = size;
            _faceList.insert(_faceList.begin(), std::move(fontFace));
        }

        int maxCount = _texWidth / (face->size->metrics.max_advance / 64);
        CharSlotArray* newSlot = (*_faceList.begin())->_slotList.insert(
            (*_faceList.begin())->_slotList.begin(), 
            _texHeap->CreateCharSlotArray(size, maxCount))->get();

        ClearFontTextureRegion(newSlot->GetSlotHeight(), newSlot->GetSlotHeightEnd());
        return _faceList.begin()->get();
    }
}

FontCharID FT_FontTextureMgr::FontFace::FindEmptyCharSlot(const FontChar& fc, int *x, int *y, FontTexKind kind)
{
    CharSlotArray *slot = NULL;
    unsigned slotIndex = ~unsigned(0x0);
    CharSlotArrayList::iterator slotIter = _slotList.begin();
    for( ; slotIter != _slotList.end(); ++slotIter) {
        CharSlotArray *cursor = (*slotIter).get();
        if(!cursor->IsFull()) {
            slot = cursor;
            slotIndex = (unsigned)std::distance(_slotList.begin(), slotIter);
            break;
        }
    }

    if(!slot) {
        if(!_texHeap->IsAllocable(_size)) {
            // slot = OverwriteLRUChar(*this, kind);
            // if(!slot) {
            //     // all character used current frame, will be reset
            //     return false;
            // }
            return FontCharID_Invalid;
        } else {
            int maxCount = _texWidth / (_face->size->metrics.max_advance / 64);
                // DavidJ -- note --    we have to add on to the back here. The _table member of
                //                      FontFace keeps indices into the _slotList array. So we can't
                //                      change the position of existing slot arrays
            _slotList.push_back(_texHeap->CreateCharSlotArray(_size, maxCount));
            slotIndex = _slotList.size()-1;
            slot = _slotList[slotIndex].get();

            // ClearFontTextureRegion(slot->GetSlotHeight(), slot->GetSlotHeightEnd());
        }
    }

    FontCharID result = slot->UseEmptySlot(fc);
    if (result != FontCharID_Invalid) {
        FontChar* newChar = slot->GetChar(result);
        int offsetX = result * (_face->size->metrics.max_advance / 64);
        int offsetY = slot->GetSlotHeight();
        newChar->u0 = (float)offsetX / _texWidth;
        newChar->v0 = (float)offsetY / _texHeight;
        newChar->u1 = (float)(offsetX + _face->glyph->bitmap.width) / _texWidth;
        newChar->v1 = (float)(offsetY + _face->glyph->bitmap.rows) / _texHeight;
        newChar->offsetX = offsetX;
        newChar->offsetY = offsetY;

        *x = offsetX;
        *y = offsetY;

        return (result & 0xffff) | (slotIndex << 16);
    }

    return FontCharID_Invalid;
}

FT_FontTextureMgr::FontFace::FontFace(TextureHeap* texHeap, FontTexture2D* texture, int texWidth, int texHeight)
: _texHeap(texHeap), _texture(texture), _texWidth(texWidth), _texHeight(texHeight)
{
}

CharSlotArray* FT_FontTextureMgr::OverwriteLRUChar(FontFace* face, FontTexKind kind)
{
    CharSlotArrayList::iterator it = face->_slotList.begin();

    CharSlotArray* minSlot = (*it).get();
    const FontChar* minChar = minSlot->GetLeastUsedChar();

    ++it;

    for( ; it != face->_slotList.end(); ++it) {        
        CharSlotArray* curSlot = (*it).get();
        const FontChar* curChar = curSlot->GetLeastUsedChar();
        if (minChar && curChar) {
            if (minChar->usedTime > curChar->usedTime) {
                minSlot = curSlot;
                minChar = curChar;
            }
        } else if (!minChar) {
            minSlot = curSlot;
            minChar = curChar;
        }
    }

    assert(minChar);

    // float minTime = minChar->usedTime;
    // if(minTime - 0.001f <= desktop.time && desktop.time <= minTime + 0.001f) {
    //     // all character is used current time, will be reset
    //     return NULL;
    // }

    // if(RequestSacrificeChar(face->_face, minChar->ch, kind)) {
    //     return minSlot;
    // }

    //cannot be reached
    // GameWarning("OverwriteLRUChar - cannot be reached");
    return NULL;
}

void FT_FontTextureMgr::ClearFontTextureRegion(int height, int heightEnd)
{
    int texHeight = heightEnd - height;
    if(texHeight <= 0) {
        return;
    }

    if(heightEnd > _texHeight) {
        return;
    }

    auto blankBuf = BufferUploads::CreateBasicPacket(_texWidth * texHeight, nullptr, std::make_pair(_texWidth, _texWidth*texHeight));
    XlSetMemory(const_cast<void*>(blankBuf->GetData(0,0)), 0, _texWidth * texHeight);
    _texture->UpdateToTexture(blankBuf.get(), 0, height, _texWidth, texHeight);
}

void FT_FontTextureMgr::FontFace::DeleteChar(FontCharID fc)
{
    unsigned slotIndex = fc >> 16;
    if (slotIndex < _slotList.size()) {
        CharSlotArray* slot = _slotList[slotIndex].get();
        slot->ReleaseSlot(fc & 0xffff);
    }
}

const FontChar* FT_FontTextureMgr::FontFace::GetChar(int ch, FontTexKind kind)
{
    FontCharID& id = _table[ch];
    if (id == FontCharID_Invalid) {
        id = CreateChar(ch, kind);
    }

    unsigned slotIndex = id >> 16;
    if (slotIndex < _slotList.size()) {
        auto* result = _slotList[slotIndex]->GetChar(id & 0xffff);
        assert(result->ch == ch);
        return result;
    }
    return NULL;
}

void FT_FontTextureMgr::DeleteFontFace(FTFont* font)
{
    FontFace *face = FindFontFace(font->GetFace(), font->GetSize());
    if(face) {
        CharSlotArrayList::iterator it = face->_slotList.begin();
        for( ; it != face->_slotList.end(); ++it) {
            _texHeap->DestroyCharSlotArray((*it).get());
        }

            //
            //      operator==( std::unique_ptr<A>, A* ) comparison is not defined
            //      So we need to use a lambda to explicitly do the comparison
            //
        _faceList.erase(
            std::remove_if(_faceList.begin(), _faceList.end(), 
                [=](FontFaceList::value_type& it) { return it.get() == face; }));
    }
}

////////////////////////////////////////////////////////////////////////////////

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

FontCharID& FT_FontTextureMgr::FontCharTable::operator[](ucs4 ch)
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
    auto i = std::lower_bound(list.begin(), list.end(), ch, CompareFirst<ucs4, FontCharID>());
    if (i == list.end() || i->first != ch) {
        auto i2 = list.insert(i, std::make_pair(ch, FontCharID_Invalid));
        return i2->second;
    }
    return i->second;
}

}

