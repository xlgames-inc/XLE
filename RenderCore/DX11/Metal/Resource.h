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

namespace RenderCore { class Resource; }
namespace RenderCore { namespace Metal_DX11
{
	class DeviceContext;
	class ObjectFactory;

    namespace Underlying
    {
        typedef ID3D::Resource      Resource;
    }

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

#if 0
	class Resource : public intrusive_ptr<Underlying::Resource>
	{
	public:
		using Desc = ResourceDesc;

		void SetImageLayout(
			DeviceContext& context, ImageLayout oldLayout, ImageLayout newLayout);

		Resource(
			const ObjectFactory& factory, const Desc& desc,
			const void* initData = nullptr, size_t initDataSize = 0);
		Resource();
		~Resource();

		Underlying::Resource* GetImage() { return get(); }
		Underlying::Resource* GetBuffer() { return get(); }
	};
#endif

	/// <summary>Helper object to catch multiple similar pointers</summary>
	/// To help with platform abstraction, RenderCore::Resource* is actually the
	/// same as a Metal::Resource*. This helper allows us to catch both equally.
	class UnderlyingResourcePtr
	{
	public:
		Underlying::Resource* get() { return _res; }

		// UnderlyingResourcePtr(Resource* res) { _res = res->GetImage(); }
		UnderlyingResourcePtr(RenderCore::Resource* res) { _res = (Underlying::Resource*)res; }
		UnderlyingResourcePtr(const RenderCore::ResourcePtr& res) { _res = (Underlying::Resource*)res.get(); }
		UnderlyingResourcePtr(Underlying::Resource* res) { _res = res; }
		UnderlyingResourcePtr(Underlying::Resource& res) { _res = &res; }
		UnderlyingResourcePtr(intrusive_ptr<Underlying::Resource> res) { _res = res.get(); }
	protected:
		Underlying::Resource* _res;
	};

    class DeviceContext;

	void Copy(
		DeviceContext&, UnderlyingResourcePtr dst, UnderlyingResourcePtr src, 
		ImageLayout dstLayout = ImageLayout::Undefined, ImageLayout srcLayout = ImageLayout::Undefined);

    namespace Internal { static std::true_type UnsignedTest(unsigned); static std::false_type UnsignedTest(...); }

    class PixelCoord
    {
    public:
        unsigned _x, _y, _z;
        PixelCoord(unsigned x=0, unsigned y=0, unsigned z=0)    { _x = x; _y = y; _z = z; }

            // We can initialize from anything that looks like a collection of unsigned values
            // This is a simple way to get casting from XLEMath::UInt2 (etc) types without
            // having to include XLEMath headers from here.
            // Plus, it will also work with any other types that expose a stl collection type
            // interface.
		template<
			typename Source,
			typename InternalTestType = decltype(Internal::UnsignedTest(std::declval<typename Source::value_type>())),
			std::enable_if<InternalTestType::value>* = nullptr>
            PixelCoord(const Source& src)
            {
                auto size = std::size(src);
                unsigned c=0;
                for (; c<std::min(unsigned(size), 3u); ++c) ((unsigned*)this)[c] = src[c];
                for (; c<3u; ++c) ((unsigned*)this)[c] = 0u;
            }
    };

    class CopyPartial_Dest
    {
    public:
        ID3D::Resource* _resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;

        CopyPartial_Dest(
            ID3D::Resource* dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord())
        : _resource(dst), _subResource(subres), _leftTopFront(leftTopFront) {}
    };

    class CopyPartial_Src
    {
    public:
        ID3D::Resource* _resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;
        PixelCoord      _rightBottomBack;

        CopyPartial_Src(
            ID3D::Resource* dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord(~0u,0,0),
            const PixelCoord rightBottomBack = PixelCoord(~0u,1,1))
        : _resource(dst), _subResource(subres)
        , _leftTopFront(leftTopFront)
        , _rightBottomBack(rightBottomBack) {}
    };

    void CopyPartial(DeviceContext&, const CopyPartial_Dest& dst, const CopyPartial_Src& src);

    intrusive_ptr<ID3D::Resource> Duplicate(DeviceContext& context, ID3D::Resource* inputResource);

	void SetImageLayout(
		DeviceContext& context, UnderlyingResourcePtr res,
		ImageLayout oldLayout, ImageLayout newLayout);

	ResourceDesc ExtractDesc(UnderlyingResourcePtr res);

	ID3D::Resource* AsID3DResource(UnderlyingResourcePtr);
	RenderCore::ResourcePtr AsResourcePtr(ID3D::Resource*);
	RenderCore::ResourcePtr AsResourcePtr(intrusive_ptr<ID3D::Resource>&&);
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

    extern template Utility::intrusive_ptr<RenderCore::Metal_DX11::Underlying::Resource>;

        /// \endcond
#pragma warning(pop)

