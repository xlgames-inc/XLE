// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/IteratorUtils.h"
#include <vector>

#include "Metal/Forward.h"      // (for Metal::ShaderResourceView)

namespace RenderCore 
{
    class MiniInputElementDesc;
    class ConstantBufferView;
    enum class Format;

    class UniformsStream
    {
    public:
        // todo -- is there any way to shift ShaderResourceView down to RenderCore layer?
        IteratorRange<const ConstantBufferView*> _constantBuffers = {};
        IteratorRange<const Metal::ShaderResourceView*const*> _resources = {};
    };

    class ConstantBufferElement
    {
    public:
        uint64_t    _semanticHash = 0ull;
        Format      _nativeFormat = Format(0);
        unsigned    _arrayElementCount = 1u;
    };

    unsigned CalculateStride(IteratorRange<const ConstantBufferElement*>);

    class UniformsStreamInterface
    {
    public:
        struct CBBinding
        {
        public:
            uint64_t _hashName;
            IteratorRange<const ConstantBufferElement*> _elements = {};
        };

        void BindConstantBuffer(unsigned slot, const CBBinding& binding);
        void BindShaderResource(unsigned slot, uint64_t hashName);

        UniformsStreamInterface();
        ~UniformsStreamInterface();

    ////////////////////////////////////////////////////////////////////////
        struct SavedCBBinding
        {
        public:
            uint64_t _hashName;
            std::vector<ConstantBufferElement> _elements;
        };
        using SlotIndex = unsigned;
        std::vector<std::pair<SlotIndex, SavedCBBinding>> _cbBindings;
        std::vector<std::pair<SlotIndex, uint64_t>> _srvBindings;
    };
    
}


