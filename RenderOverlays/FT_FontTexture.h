// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FontPrimitives.h"
#include "../RenderCore/Format.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/IntrusivePtr.h"
#include <vector>
#include <memory>

namespace RenderCore { class IResource; class Box2D; }
namespace BufferUploads { class IManager; class ResourceLocator; class DataPacket; using TransactionID = uint64_t; }

typedef struct FT_FaceRec_ FT_FaceRec;
typedef struct FT_FaceRec_* FT_Face;

namespace RenderOverlays
{
	class FontTexture2D
	{
	public:
		void UpdateToTexture(BufferUploads::DataPacket& packet, const RenderCore::Box2D& destBox);
		const std::shared_ptr<RenderCore::IResource>& GetUnderlying() const;

		FontTexture2D(unsigned width, unsigned height, RenderCore::Format pixelFormat);
		~FontTexture2D();

		FontTexture2D(FontTexture2D&&) = default;
		FontTexture2D& operator=(FontTexture2D&&) = default;

	private:
		mutable BufferUploads::TransactionID					_transaction;
		mutable intrusive_ptr<BufferUploads::ResourceLocator>	_locator;
	};

	class FT_FontTextureMgr
	{
		class Pimpl;
	public:
		// bool            Init(int texWidth, int texHeight);

		// bool            IsNeedReset() const;
		// void            RequestReset();
		// void            Reset();

		// typedef std::vector<std::unique_ptr<CharSlotArray>> CharSlotArrayList;
		struct FontCharTable
		{
			std::vector<std::vector<std::pair<ucs4, FontGlyphID>>>  _table;
			FontGlyphID&         operator[](ucs4 ch);
			void                ClearTable();
			FontCharTable();
			~FontCharTable();
		};

		class FontFace
		{
		public:
			struct Glyph
			{
				int ch;
				float u0 = 0.f, v0 = 0.f;
				float u1 = 1.f, v1 = 1.f;
				float left = 0.f, top = 0.f;
				float width = 0.f, height = 0.f;
				float xAdvance = 0.f;

				int offsetX = 0, offsetY = 0;
			};

			const Glyph*	GetChar(int ch);
			FontGlyphID		CreateChar(int ch);

			// void                DeleteChar(FontGlyphID fc);
			// const FontTexture2D*    GetTexture() const { return _texture; }

			FontFace(FT_Face face, int faceSize, const std::shared_ptr<Pimpl>& pimpl);
			~FontFace();
			FontFace(FontFace&&) = default;
			FontFace& operator=(FontFace&&) = default;
    
			/*CharSlotArrayList   _slotList;*/

		private:
			FontCharTable       _table;
			
			FT_Face _face;
			int _faceSize;

			/*int                 _texWidth, _texHeight;
			TextureHeap *       _texHeap;
			FontTexture2D *     _texture;

			FontGlyphID          FindEmptyCharSlot(const FontChar& fc, int *x, int *y);*/

			std::shared_ptr<Pimpl> _pimpl;
		};

		std::shared_ptr<FontFace>	FindFontFace(FT_Face face, int size);
		std::shared_ptr<FontFace>	CreateFontFace(FT_Face face, int size);

		FT_FontTextureMgr();
		~FT_FontTextureMgr();

	private:
		// CharSlotArray*      OverwriteLRUChar(FontFace* face);
		// void                ClearFontTextureRegion(int height, int heightEnd);

		std::shared_ptr<Pimpl> _pimpl;

		struct Entry
		{
			FT_Face _ftFace;
			int _faceSize;
			std::weak_ptr<FontFace> _face;
		};
		std::vector<Entry> _faces;
	};

}

