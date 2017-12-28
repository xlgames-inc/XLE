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

    class UniformsStream
    {
    public:
        // todo -- is there any way to shift ShaderResourceView down to RenderCore layer?
        IteratorRange<const ConstantBufferView*> _constantBuffers = {};
        IteratorRange<const Metal::ShaderResourceView**> _resources = {};
    };

    class UniformsStreamInterface
    {
    public:
        struct CBBinding
        {
        public:
            uint64_t _hashName;
            IteratorRange<const MiniInputElementDesc*> _elements = {};
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
            std::vector<MiniInputElementDesc> _elements;
        };
        using SlotIndex = unsigned;
        std::vector<std::pair<SlotIndex, SavedCBBinding>> _cbBindings;
        std::vector<std::pair<SlotIndex, uint64_t>> _srvBindings;
    };
    
}


