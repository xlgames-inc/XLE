// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Matrix.h"
#include <memory>

namespace RenderCore { namespace Assets { class SharedStateSet; class ModelRenderer; class ModelScaffold; }}
namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    /// <summary>Prepares imposter "sprites" for objects, and uses them as a stand-in</summary>
    /// Far-off objects can be approximated with sprites (though often a single object may need
    /// several sprites for different angles and lighting conditions).
    ///
    /// In this case, the sprites are dynamically created as required.
    class DynamicImposters
    {
    public:
        using SharedStateSet = RenderCore::Assets::SharedStateSet;
        using ModelRenderer = RenderCore::Assets::ModelRenderer;
        using ModelScaffold = RenderCore::Assets::ModelScaffold;

        void Render(
            RenderCore::Metal::DeviceContext& context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);

        void Queue(
            const ModelRenderer& renderer, const ModelScaffold& scaffold, 
            const Float3x4& localToWorld, const Float3& cameraPosition);
        void Reset();

        DynamicImposters(SharedStateSet& sharedStateSet);
        ~DynamicImposters();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

