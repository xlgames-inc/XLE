// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Assets/AssetUtils.h"
#include "../Utility/ParameterBox.h"

namespace Assets { class DependencyValidation; class DirectorySearchRules; }
namespace Utility { class Data; }

namespace RenderCore { namespace ColladaConversion
{
    class MaterialSettingsFile
    {
    public:
        class MaterialDesc
        {
        public:
            ParameterBox _matParamBox;
            Assets::MaterialParameters::ResourceBindingSet _resourceBindings;
            Assets::RenderStateSet _stateSet;

            MaterialDesc();
            ~MaterialDesc();
            MaterialDesc(
                const Utility::Data& source,
                ::Assets::DirectorySearchRules* searchRules = nullptr,
                std::vector<const ::Assets::DependencyValidation*>* inherited = nullptr);
        };
        std::vector<std::pair<uint64, MaterialDesc>> _materials;
        
        const ::Assets::DependencyValidation& GetDependencyValidation() const { return *_depVal; }
        
        MaterialSettingsFile();
        MaterialSettingsFile(const ResChar filename[]);
        ~MaterialSettingsFile();
    private:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };
}}

