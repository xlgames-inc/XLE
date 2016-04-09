// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../Utility/IntrusivePtr.h"

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

	class Resource : public intrusive_ptr<Underlying::Resource>
	{
	public:
		using Desc = BufferUploads::BufferDesc;

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

    class DeviceContext;

    void Copy(DeviceContext&, ID3D::Resource* dst, ID3D::Resource* src);
	void Copy(DeviceContext&, Resource& dst, Resource& src, ImageLayout dstLayout, ImageLayout srcLayout);

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

