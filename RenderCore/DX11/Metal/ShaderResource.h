// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "Format.h"
#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { namespace Metal_DX11
{
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
        typedef ID3D::Resource* UnderlyingResource;
        explicit ShaderResourceView(UnderlyingResource resource, NativeFormat::Enum format = NativeFormat::Unknown, int arrayCount=0, bool forceSingleSample=false);
        explicit ShaderResourceView(UnderlyingResource resource, NativeFormat::Enum format, const MipSlice& mipSlice);
        explicit ShaderResourceView(ID3D::ShaderResourceView* resource);
        explicit ShaderResourceView(MovePTRHelper<ID3D::ShaderResourceView> resource);
        ShaderResourceView();
        ~ShaderResourceView();

        ShaderResourceView(const ShaderResourceView& cloneFrom);
        ShaderResourceView(ShaderResourceView&& moveFrom) never_throws;
        ShaderResourceView& operator=(const ShaderResourceView& cloneFrom);
        ShaderResourceView& operator=(ShaderResourceView&& moveFrom) never_throws;

        intrusive_ptr<ID3D::Resource>           GetResource() const;
        
        typedef ID3D::ShaderResourceView*       UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::ShaderResourceView>   _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    
}}

