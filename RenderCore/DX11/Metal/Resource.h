// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "../../IDevice.h"
#include "../../ResourceDesc.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { class IResource; }
namespace RenderCore { namespace Metal_DX11
{
	class DeviceContext;
	class ObjectFactory;

	enum class ImageLayout
	{
		Undefined,
		General,
		ColorAttachmentOptimal,
		DepthStencilAttachmentOptimal,
		DepthStencilReadOnlyOptimal,
		ShaderReadOnlyOptimal,
		TransferSrcOptimal,
		TransferDstOptimal,
		Preinitialized,
		PresentSrc
	};

	/// <summary>Helper object to catch multiple similar pointers</summary>
	/// To help with platform abstraction, RenderCore::Resource* is actually the
	/// same as a Metal::Resource*. This helper allows us to catch both equally.
	using UnderlyingResourcePtr = intrusive_ptr<ID3D::Resource>;

	class Resource : public IResource
	{
	public:
		UnderlyingResourcePtr _underlying;

		virtual void*			QueryInterface(size_t guid);
		virtual ResourceDesc	GetDesc() const;

		Resource() {}
		explicit Resource(const UnderlyingResourcePtr& underlying) : _underlying(underlying) {}
		explicit Resource(UnderlyingResourcePtr&& underlying) : _underlying(std::move(underlying)) {}
	};

    class DeviceContext;
    class ShaderResourceView;
    class DepthStencilView;
    class RenderTargetView;
    class UnorderedAccessView;

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      C O P Y I N G       //
///////////////////////////////////////////////////////////////////////////////////////////////////

	void Copy(
		DeviceContext&, UnderlyingResourcePtr dst, UnderlyingResourcePtr src, 
		ImageLayout dstLayout = ImageLayout::Undefined, ImageLayout srcLayout = ImageLayout::Undefined);

    using UInt3Pattern = VectorPattern<unsigned, 3>;

    class CopyPartial_Dest
    {
    public:
        ID3D::Resource* _resource;
        SubResourceId   _subResource;
        UInt3Pattern    _leftTopFront;

        CopyPartial_Dest(
            UnderlyingResourcePtr dst, SubResourceId subres = {},
            const UInt3Pattern& leftTopFront = UInt3Pattern())
        : _resource(dst.get()), _subResource(subres), _leftTopFront(leftTopFront) {}
    };

    class CopyPartial_Src
    {
    public:
        ID3D::Resource* _resource;
        SubResourceId   _subResource;
        UInt3Pattern    _leftTopFront;
        UInt3Pattern    _rightBottomBack;

        CopyPartial_Src(
            UnderlyingResourcePtr dst, SubResourceId subres = {},
            const UInt3Pattern& leftTopFront = UInt3Pattern(~0u,0,0),
            const UInt3Pattern& rightBottomBack = UInt3Pattern(~0u,1,1))
        : _resource(dst.get()), _subResource(subres)
        , _leftTopFront(leftTopFront)
        , _rightBottomBack(rightBottomBack) {}
    };

    void CopyPartial(
        DeviceContext&, const CopyPartial_Dest& dst, const CopyPartial_Src& src,
        ImageLayout dstLayout = ImageLayout::Undefined, ImageLayout srcLayout = ImageLayout::Undefined);

    intrusive_ptr<ID3D::Resource> Duplicate(DeviceContext& context, UnderlyingResourcePtr inputResource);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      G E T   D E S C       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    ResourceDesc ExtractDesc(const UnderlyingResourcePtr& res);
	ResourceDesc ExtractDesc(const IResource& res);
	ResourceDesc ExtractDesc(const ShaderResourceView& res);
	ResourceDesc ExtractDesc(const RenderTargetView& res);
    ResourceDesc ExtractDesc(const DepthStencilView& res);
    ResourceDesc ExtractDesc(const UnorderedAccessView& res);
    std::shared_ptr<IResource> ExtractResource(const ShaderResourceView&);
	std::shared_ptr<IResource> ExtractResource(const RenderTargetView&);
	std::shared_ptr<IResource> ExtractResource(const DepthStencilView&);
    std::shared_ptr<IResource> ExtractResource(const UnorderedAccessView&);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      U T I L S       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    class LayoutTransition
    {
    public:
        UnderlyingResourcePtr _res;
		ImageLayout _oldLayout, _newLayout;

        LayoutTransition(
            UnderlyingResourcePtr res = nullptr, 
            ImageLayout oldLayout = ImageLayout::Undefined,
            ImageLayout newLayout = ImageLayout::Undefined) 
            : _res(res), _oldLayout(oldLayout), _newLayout(newLayout) {}
    };
    inline void SetImageLayouts(DeviceContext& context, IteratorRange<const LayoutTransition*> changes) {}

	/////////////// Resource creation and access ///////////////

	using ResourceInitializer = std::function<SubResourceInitData(SubResourceId)>;
	std::shared_ptr<IResource> CreateResource(
		const ObjectFactory& factory,
		const ResourceDesc& desc, 
		const ResourceInitializer& init = ResourceInitializer());

	intrusive_ptr<ID3D::Resource> CreateUnderlyingResource(
		const ObjectFactory& factory,
		const ResourceDesc& desc,
		const ResourceInitializer& init = ResourceInitializer());

	ID3D::Resource* AsID3DResource(UnderlyingResourcePtr);
	ID3D::Resource* AsID3DResource(IResource&);
	std::shared_ptr<IResource> AsResourcePtr(ID3D::Resource*);
	std::shared_ptr<IResource> AsResourcePtr(intrusive_ptr<ID3D::Resource>&&);
}}

#pragma warning(push)
#pragma warning(disable:4231)   // nonstandard extension used : 'extern' before template explicit instantiation

        /// \cond INTERNAL
        //
        //      Implement the intrusive_ptr for resources in only one CPP file. 
        //      Note this means that AddRef/Release are pulled out-of-line
        //
        //      But this is required, otherwise each use of intrusive_ptr<Render::Underlying::Resource>
        //      requires #include <d3d11.h>
        //

    extern template Utility::intrusive_ptr<ID3D::Resource>;

        /// \endcond
#pragma warning(pop)

