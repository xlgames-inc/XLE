// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Format.h"
#include "../Utility/IntrusivePtr.h"
#include <memory>

namespace RenderCore { class IResource; class Box2D; }
namespace BufferUploads { class IManager; class ResourceLocator; class DataPacket; using TransactionID = uint64_t; }

namespace RenderOverlays
{
	class FontTexture2D
	{
	public:
		void UpdateToTexture(BufferUploads::DataPacket& packet, const RenderCore::Box2D& destBox);
		const std::shared_ptr<RenderCore::Resource>& GetUnderlying() const;

		FontTexture2D(unsigned width, unsigned height, RenderCore::Format pixelFormat);
		~FontTexture2D();

		FontTexture2D(FontTexture2D&&) = default;
		FontTexture2D& operator=(FontTexture2D&&) = default;

	private:
		mutable BufferUploads::TransactionID					_transaction;
		mutable intrusive_ptr<BufferUploads::ResourceLocator>	_locator;
	};
}

