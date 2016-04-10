// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "Resource.h"
#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { enum class Format; }
namespace RenderCore { namespace Metal_DX11
{
	class ObjectFactory;

    class MipSlice
    {
    public:
        unsigned _mostDetailedMip;
        unsigned _mipLevels;
        MipSlice(unsigned mostDetailedMip = 0, unsigned mipLevels = 1) : _mostDetailedMip(mostDetailedMip), _mipLevels(mipLevels) {}
    };

    class ShaderResourceView
    {
    public:
        explicit ShaderResourceView(UnderlyingResourcePtr resource, Format format = Format(0), int arrayCount=0, bool forceSingleSample=false);
        ShaderResourceView(UnderlyingResourcePtr resource, Format format, const MipSlice& mipSlice);
		ShaderResourceView(const ObjectFactory& factory, UnderlyingResourcePtr resource, Format format = Format(0), int arrayCount = 0, bool forceSingleSample = false);
		ShaderResourceView(const ObjectFactory& factory, UnderlyingResourcePtr resource, Format format, const MipSlice& mipSlice);
        explicit ShaderResourceView(intrusive_ptr<ID3D::ShaderResourceView>&& resource);
        explicit ShaderResourceView(MovePTRHelper<ID3D::ShaderResourceView> resource);
        ShaderResourceView();
        ~ShaderResourceView();

        ShaderResourceView(const ShaderResourceView& cloneFrom);
        ShaderResourceView(ShaderResourceView&& moveFrom) never_throws;
        ShaderResourceView& operator=(const ShaderResourceView& cloneFrom);
        ShaderResourceView& operator=(ShaderResourceView&& moveFrom) never_throws;

        static ShaderResourceView RawBuffer(ID3D::Resource& res, unsigned sizeBytes, unsigned offsetBytes = 0);

        intrusive_ptr<ID3D::Resource>           GetResource() const;
        
        typedef ID3D::ShaderResourceView*       UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::ShaderResourceView>   _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    
}}

