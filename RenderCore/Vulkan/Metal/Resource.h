// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { namespace Metal_Vulkan
{
    namespace Underlying
    {
		class DummyResource {};
        typedef DummyResource Resource;
    }

    class DeviceContext;

	inline void Copy(DeviceContext&, Underlying::Resource* dst, Underlying::Resource* src) {}

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
        Underlying::Resource* _resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;

        CopyPartial_Dest(
			Underlying::Resource* dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord())
        : _resource(dst), _subResource(subres), _leftTopFront(leftTopFront) {}
    };

    class CopyPartial_Src
    {
    public:
		Underlying::Resource* _resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;
        PixelCoord      _rightBottomBack;

        CopyPartial_Src(
			Underlying::Resource* dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord(~0u,0,0),
            const PixelCoord rightBottomBack = PixelCoord(~0u,1,1))
        : _resource(dst), _subResource(subres)
        , _leftTopFront(leftTopFront)
        , _rightBottomBack(rightBottomBack) {}
    };

	inline void CopyPartial(DeviceContext&, const CopyPartial_Dest& dst, const CopyPartial_Src& src) {}

	inline intrusive_ptr<Underlying::Resource> Duplicate(DeviceContext& context, Underlying::Resource* inputResource) { return nullptr; }
}}

namespace Utility
{
	template<> inline void intrusive_ptr_add_ref(RenderCore::Metal_Vulkan::Underlying::Resource* p) {}
	template<> inline void intrusive_ptr_release(RenderCore::Metal_Vulkan::Underlying::Resource* p) {}
}