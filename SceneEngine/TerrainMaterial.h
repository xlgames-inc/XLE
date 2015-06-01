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

namespace SceneEngine
{
    class TerrainMaterialScaffold
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
        UInt2 _diffuseDims;
        UInt2 _normalDims;
        UInt2 _paramDims;

        ::Assets::DirectorySearchRules _searchRules;

        mutable uint64 _cachedHashValue;
        uint64 GetHash() const;

        void Write(OutputStream& stream) const;

        TerrainMaterialScaffold();
        TerrainMaterialScaffold(const char definitionFile[]);
        ~TerrainMaterialScaffold();

        static std::unique_ptr<TerrainMaterialScaffold> CreateNew(const char definitionFile[] = nullptr);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };
}

