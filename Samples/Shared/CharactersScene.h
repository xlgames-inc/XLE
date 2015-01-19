// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/Forward.h"
#include "../../SceneEngine/LightingParserContext.h"
#include <memory>

namespace Sample
{
    class PlayerCharacter;
    class CharactersScene
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext,
            int techniqueIndex) const;
        void Prepare(
            RenderCore::Metal::DeviceContext* context);
        void Cull(const Float4x4& worldToProjection);
        void Update(float deltaTime);

        std::shared_ptr<PlayerCharacter>  GetPlayerCharacter();

        Float4x4 DefaultCameraToWorld() const;

        CharactersScene();
        ~CharactersScene();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}