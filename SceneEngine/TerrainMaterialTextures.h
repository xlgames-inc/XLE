// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../Assets/Assets.h"

namespace SceneEngine
{
    class TerrainMaterialScaffold;

    class TerrainMaterialTextures
    {
    public:
        enum Resources { Diffuse, Normal, Specularity, ResourceCount };
        intrusive_ptr<ID3D::Resource> _textureArray[ResourceCount];
        RenderCore::Metal::ShaderResourceView _srv[ResourceCount];
        RenderCore::Metal::ConstantBuffer _texturingConstants;
        RenderCore::Metal::ConstantBuffer _procTexContsBuffer;
        unsigned _strataCount;

        TerrainMaterialTextures();
        TerrainMaterialTextures(const TerrainMaterialScaffold& scaffold, bool useGradFlagMaterials = true);
        ~TerrainMaterialTextures();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };
}

