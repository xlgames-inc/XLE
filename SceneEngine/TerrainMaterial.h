// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/Assets.h"
#include "../Assets/AssetUtils.h"
#include "../Math/Vector.h"
#include "../Core/Types.h"
#include <vector>

namespace Utility { class OutputStream; }

namespace SceneEngine
{
    class TerrainMaterialScaffold
    {
    public:
        class StrataMaterial
        {
        public:
            class Strata
            {
            public:
                ::Assets::rstring _texture[3];
                float _mappingConstant[3];
                float _endHeight;
            };
            std::vector<Strata> _strata;
            unsigned _id;
        };

        class GradFlagMaterial
        {
        public:
            ::Assets::rstring _texture[5];
            float _mappingConstant[5];
            unsigned _id;
        };

        class ProcTextureSetting
        {
        public:
            ::Assets::rstring _name;
            ::Assets::rstring _texture[2];
            float _hgrid, _gain;

            ProcTextureSetting() : _hgrid(100.f), _gain(.5f) {}
        };

        UInt2 _diffuseDims;
        UInt2 _normalDims;
        UInt2 _paramDims;
        std::vector<StrataMaterial> _strataMaterials;
        std::vector<GradFlagMaterial> _gradFlagMaterials;
        std::vector<ProcTextureSetting> _procTextures;

        ::Assets::DirectorySearchRules _searchRules;

        void Write(Utility::OutputStream& stream) const;

        TerrainMaterialScaffold();
        TerrainMaterialScaffold(const char definitionFile[]);
        ~TerrainMaterialScaffold();

        static std::unique_ptr<TerrainMaterialScaffold> CreateNew(const char definitionFile[] = nullptr);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };
}

