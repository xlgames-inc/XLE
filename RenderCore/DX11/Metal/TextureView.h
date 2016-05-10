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
    class DeviceContext;

    class TextureViewWindow
    {
    public:
        struct SubResourceRange { unsigned _min; unsigned _count; };
        static const unsigned Unlimited = ~0x0u;
        static const SubResourceRange All;

		struct Flags
        {
			enum Bits 
            { 
                AttachedCounter = 1<<0, AppendBuffer = 1<<1, 
                ForceArray = 1<<2, ForceSingleSample = 1<<3, 
                JustDepth = 1<<4, JustStencil = 1<<5 
            };
			using BitField = unsigned;
		};

        struct FormatFilter
        {
            enum Aspect { UndefinedAspect, Color, Depth, Stencil, DepthStencil };
            enum ColorSpace { UndefinedColorSpace, Linear, SRGB };

            ColorSpace  _colorSpace;
            Aspect      _aspect;
            Format      _explicitFormat;

            FormatFilter(ColorSpace colorSpace = UndefinedColorSpace, Aspect aspect = UndefinedAspect)
                : _colorSpace(colorSpace), _aspect(aspect), _explicitFormat(Format(0)) {}
            FormatFilter(Format explicitFormat) : _colorSpace(UndefinedColorSpace), _aspect(UndefinedAspect), _explicitFormat(explicitFormat) {}
        };

        FormatFilter                _format;
        SubResourceRange            _mipRange;
        SubResourceRange            _arrayLayerRange;
        TextureDesc::Dimensionality _dimensionality;
		Flags::BitField				_flags;

        TextureViewWindow(
            FormatFilter format = FormatFilter(),
            TextureDesc::Dimensionality dimensionality = TextureDesc::Dimensionality::Undefined,
            SubResourceRange mipRange = All,
            SubResourceRange arrayLayerRange = All,
			Flags::BitField flags = 0
            ) : _format(format), _dimensionality(dimensionality), _mipRange(mipRange), _arrayLayerRange(arrayLayerRange), _flags(flags) {}
    };
    
    class RenderTargetView
    {
    public:
        RenderTargetView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());
        explicit RenderTargetView(UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());

        RenderTargetView(ID3D::RenderTargetView* resource);
        RenderTargetView(MovePTRHelper<ID3D::RenderTargetView> resource);
        RenderTargetView(DeviceContext& context);
        RenderTargetView();
        ~RenderTargetView();

        RenderTargetView(const RenderTargetView& cloneFrom);
        RenderTargetView(RenderTargetView&& moveFrom) never_throws;
        RenderTargetView& operator=(const RenderTargetView& cloneFrom);
        RenderTargetView& operator=(RenderTargetView&& moveFrom) never_throws;
        
        intrusive_ptr<ID3D::Resource>	GetResource() const;
        ResourcePtr		                ShareResource() const;

        typedef ID3D::RenderTargetView*         UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::RenderTargetView>      _underlying;
    };

    class DepthStencilView
    {
    public:
        DepthStencilView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());
        explicit DepthStencilView(UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());

        DepthStencilView(ID3D::DepthStencilView* resource);
        DepthStencilView(MovePTRHelper<ID3D::DepthStencilView> resource);
        DepthStencilView(DeviceContext& context);
        DepthStencilView();
        ~DepthStencilView();

        DepthStencilView(const DepthStencilView& cloneFrom);
        DepthStencilView(DepthStencilView&& moveFrom) never_throws;
        DepthStencilView& operator=(const DepthStencilView& cloneFrom);
        DepthStencilView& operator=(DepthStencilView&& moveFrom) never_throws;
        
        intrusive_ptr<ID3D::Resource>	GetResource() const;
        ResourcePtr		                ShareResource() const;

        typedef ID3D::DepthStencilView*         UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::DepthStencilView>      _underlying;
    };

    class UnorderedAccessView
    {
    public:
        UnorderedAccessView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());
        explicit UnorderedAccessView(UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());

        UnorderedAccessView();
        ~UnorderedAccessView();

        UnorderedAccessView(const UnorderedAccessView& cloneFrom);
        UnorderedAccessView(UnorderedAccessView&& moveFrom) never_throws;
        UnorderedAccessView& operator=(const UnorderedAccessView& cloneFrom);
        UnorderedAccessView& operator=(UnorderedAccessView&& moveFrom) never_throws;
        
        intrusive_ptr<ID3D::Resource>   GetResource() const;
        ResourcePtr		                ShareResource() const;

        typedef ID3D::UnorderedAccessView*      UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::UnorderedAccessView>   _underlying;
    };

    class ShaderResourceView
    {
    public:
        ShaderResourceView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());
        explicit ShaderResourceView(UnderlyingResourcePtr resource, const TextureViewWindow& window = TextureViewWindow());

        explicit ShaderResourceView(intrusive_ptr<ID3D::ShaderResourceView>&& resource);
        explicit ShaderResourceView(MovePTRHelper<ID3D::ShaderResourceView> resource);
        ShaderResourceView();
        ~ShaderResourceView();

        ShaderResourceView(const ShaderResourceView& cloneFrom);
        ShaderResourceView(ShaderResourceView&& moveFrom) never_throws;
        ShaderResourceView& operator=(const ShaderResourceView& cloneFrom);
        ShaderResourceView& operator=(ShaderResourceView&& moveFrom) never_throws;

        static ShaderResourceView RawBuffer(UnderlyingResourcePtr res, unsigned sizeBytes, unsigned offsetBytes = 0);

		intrusive_ptr<ID3D::Resource>	GetResource() const;
        ResourcePtr		                ShareResource() const;
        
        typedef ID3D::ShaderResourceView*       UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }
        bool                                    IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::ShaderResourceView>   _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    
}}

