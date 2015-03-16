// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetUtils.h"
#include "../../Utility/ParameterBox.h"

namespace Assets { class DependencyValidation; class DirectorySearchRules; }
namespace Utility { class Data; }

namespace RenderCore { namespace Assets
{
    class RawMaterialConfiguration
    {
    public:
        Assets::MaterialParameters::ResourceBindingSet _resourceBindings;
        ParameterBox _matParamBox;
        Assets::RenderStateSet _stateSet;
        ParameterBox _constants;

        using ResString = std::basic_string<::Assets::ResChar>;
        ResString _filename;
        ResString _settingName;
        std::vector<ResString> _inherit;
        const ::Assets::DependencyValidation& GetDependencyValidation() const { return *_depVal; }

        MaterialParameters Resolve(std::vector<::Assets::FileAndTime>* deps = nullptr) const;
        
        RawMaterialConfiguration();
        RawMaterialConfiguration(const ::Assets::ResChar initialiser[]);
        ~RawMaterialConfiguration();
    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;

        void MergeInto(MaterialParameters& dest) const;
    };
}}

