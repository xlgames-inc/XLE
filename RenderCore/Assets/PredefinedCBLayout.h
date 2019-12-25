// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { class SharedPkt; }
namespace RenderCore { namespace Assets
{
    class PredefinedCBLayout
    {
    public:
        unsigned _cbSize;
        class Element
        {
        public:
            ParameterBox::ParameterNameHash _hash = ~0u;
            uint64_t _hash64 = ~0ull;
            ImpliedTyping::TypeDesc _type;
            unsigned _offset = 0;
            unsigned _arrayElementCount = 1;
            unsigned _arrayElementStride = 0;
        };
        std::vector<Element> _elements;		// note -- we hash this memory, so make this convenient, we should avoid having any pointers here
		std::vector<std::string> _elementNames;
        ParameterBox _defaults;

		struct NameAndType { std::string _name; ImpliedTyping::TypeDesc _type; unsigned _arrayElementCount = 1; };
		void AppendElements(IteratorRange<const NameAndType*> elements);

		// Reorder the given elements to try to find an ordering that will minimize the
		// size of the final constant buffer. This accounts for ordering rules such as
		// preventing vectors from crossing 16 byte boundaries.
		static void OptimizeElementOrder(IteratorRange<NameAndType*> elements);

        std::vector<uint8> BuildCBDataAsVector(const ParameterBox& parameters) const;
        SharedPkt BuildCBDataAsPkt(const ParameterBox& parameters) const;
        uint64_t CalculateHash() const;
        std::vector<ConstantBufferElementDesc> MakeConstantBufferElements() const;

        PredefinedCBLayout();
        PredefinedCBLayout(StringSection<::Assets::ResChar> initializer);
        PredefinedCBLayout(StringSection<char> source, bool);
        PredefinedCBLayout(IteratorRange<const NameAndType*> elements);
        ~PredefinedCBLayout();
        
        PredefinedCBLayout(const PredefinedCBLayout&) = default;
        PredefinedCBLayout& operator=(const PredefinedCBLayout&) = default;
        PredefinedCBLayout(PredefinedCBLayout&&) never_throws = default;
        PredefinedCBLayout& operator=(PredefinedCBLayout&&) never_throws = default;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     
            { return _validationCallback; }

    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;

        void Parse(StringSection<char> source);
        void WriteBuffer(void* dst, const ParameterBox& parameters) const;
    };
}}
