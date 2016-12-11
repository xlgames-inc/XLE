// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/Forward.h"
#include "../../RenderCore/Types_Forward.h"
#include "../../Assets/Assets.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"

namespace RenderCore { namespace Assets { class ResolvedMaterial; } }
namespace RenderCore { namespace Techniques { class ParsingContext; class PredefinedCBLayout; } }
namespace Assets { class DirectorySearchRules; }

namespace ToolsRig
{

    /// <summary>Binds material settings to the device</summary>
    /// This object is intended for tools and non-performance critical operations.
    /// For material preview operations, we need often need to bind some material
    /// settings to the device (for example, for drawing material preview window)
    /// At runtime the game will typically need to process material information
    /// into a compiled state (for example, when constructing a ModelRenderer)
    /// But for tools, we need a more flexible way to do it -- one that doesn't
    /// rely on a lot of processing steps. Hense the material binder!
    class IMaterialBinder
    {
    public:
        class SystemConstants
        {
        public:
            Float3      _lightNegativeDirection;
            Float3      _lightColour;
            Float4x4    _objectToWorld;
            SystemConstants();
        };

        virtual bool Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::InputLayout& geoInputLayout) = 0;
        virtual ~IMaterialBinder();

    protected:
        static void BindConstantsAndResources(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parsingContext,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
			const RenderCore::Metal::BoundUniforms& boundLayout,
            const RenderCore::Techniques::PredefinedCBLayout& cbLayout);
    };

    class MaterialBinder : public IMaterialBinder
    {
    public:
        virtual bool Apply(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const RenderCore::Assets::ResolvedMaterial& mat,
            const SystemConstants& sysConstants,
            const ::Assets::DirectorySearchRules& searchRules,
            const RenderCore::InputLayout& geoInputLayout);
        
        MaterialBinder(StringSection<::Assets::ResChar> shaderTypeName);
        virtual ~MaterialBinder();
    protected:
        ::Assets::rstring _shaderTypeName;
    };

}