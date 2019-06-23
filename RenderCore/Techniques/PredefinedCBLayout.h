// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/StringUtils.h"
#include <unordered_map>
#include <string>

namespace RenderCore { class SharedPkt; }
namespace RenderCore { namespace Techniques
{
    class PredefinedCBLayout
    {
    public:
        unsigned _cbSize;
        class Element
        {
        public:
            ParameterBox::ParameterNameHash _hash;
            uint64_t _hash64;
            ImpliedTyping::TypeDesc _type;
            unsigned _offset;
            unsigned _arrayElementCount;
            unsigned _arrayElementStride;
            std::string _name;
            std::string _conditions;
        };
        std::vector<Element> _elements;
        ParameterBox _defaults;

        std::vector<uint8> BuildCBDataAsVector(const ParameterBox& parameters) const;
        SharedPkt BuildCBDataAsPkt(const ParameterBox& parameters) const;
        uint64_t CalculateHash() const;
        std::vector<ConstantBufferElementDesc> MakeConstantBufferElements() const;

        PredefinedCBLayout Filter(const std::unordered_map<std::string, int>& definedTokens);

        PredefinedCBLayout();
        PredefinedCBLayout(StringSection<::Assets::ResChar> initializer);
        PredefinedCBLayout(StringSection<char> source, bool);
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

    class PredefinedCBLayoutFile
    {
    public:
        std::unordered_map<std::string, std::shared_ptr<PredefinedCBLayout>> _layouts;

        PredefinedCBLayoutFile(
            StringSection<> inputData,
            const ::Assets::DirectorySearchRules& searchRules,
            const ::Assets::DepValPtr& depVal);
        ~PredefinedCBLayoutFile();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const
            { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };
}}
