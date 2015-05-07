// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"

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
            std::string _name;
            ParameterBox::ParameterNameHash _hash;
            ImpliedTyping::TypeDesc _type;
            unsigned _offset;
        };
        std::vector<Element> _elements;
        ParameterBox _defaults;

        std::vector<uint8> BuildCBDataAsVector(const ParameterBox& parameters) const;
        SharedPkt BuildCBDataAsPkt(const ParameterBox& parameters) const;

        PredefinedCBLayout(const ::Assets::ResChar initializer[]);
        ~PredefinedCBLayout();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     
            { return _validationCallback; }

    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;

        void WriteBuffer(void* dst, const ParameterBox& parameters) const;
    };
}}
