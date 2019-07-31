// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "../../ResourceDesc.h"
#include "../../../Core/Types.h"
#include <memory>
#include <vector>

namespace RenderCore { namespace Metal_AppleMetal
{
    class DeviceContext;
    class ObjectFactory;

    class Buffer : public Resource
    {
    public:
        struct UpdateFlags
        {
            enum Enum {
                UnsynchronizedWrite = 1 << 0
            };
            using BitField = unsigned;
        };
        void Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset = 0u, UpdateFlags::BitField flags = 0u);
        bool IsGood() const { return _underlyingBuffer.get() != nullptr; }

        Buffer();
        Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                IteratorRange<const void*> initData);
        ~Buffer();
    };

    Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Resource MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Resource MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*>);

}}

