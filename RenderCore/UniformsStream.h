// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets/PredefinedDescriptorSetLayout.h"
#include "../Utility/IteratorUtils.h"
#include <vector>

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
        IteratorRange<const void*const*> _resources = {};				// Metal::ShaderResourceView
        IteratorRange<const void*const*> _samplers = {};				// Metal::SamplerState

		template<typename Type>
			static IteratorRange<const void*const*> MakeResources(const Type& input)
			{
				return IteratorRange<const void*const*>((const void*const*)input.begin(), (const void*const*)input.end());
			}
    };

    class ConstantBufferElementDesc
    {
    public:
        uint64_t    _semanticHash = 0ull;
        Format      _nativeFormat = Format(0);
        unsigned    _offset = 0u;
        unsigned    _arrayElementCount = 0u;            // set to zero if the element is not actually an array (ie, use std::max(1u, _arrayElementCount) in most cases)
    };

    class UniformsStreamInterface
    {
    public:
        struct CBBinding
        {
        public:
            uint64_t _hashName;
            IteratorRange<const ConstantBufferElementDesc*> _elements = {};
        };

        void BindConstantBuffer(unsigned slot, const CBBinding& binding);
        void BindResource(unsigned slot, uint64_t hashName);
        void BindSampler(unsigned slot, uint64_t hashName);

        uint64_t GetHash() const;

        UniformsStreamInterface();
        ~UniformsStreamInterface();

    ////////////////////////////////////////////////////////////////////////
        struct RetainedCBBinding
        {
        public:
            uint64_t _hashName = 0ull;
            std::vector<ConstantBufferElementDesc> _elements = {};
        };
        std::vector<RetainedCBBinding> _cbBindings;
        std::vector<uint64_t> _srvBindings;
        std::vector<uint64_t> _samplerBindings;

    private:
        mutable uint64_t _hash;
    };
    
    using DescriptorSetSignature = RenderCore::Assets::PredefinedDescriptorSetLayout;
    using DescriptorSet = UniformsStream;
}


