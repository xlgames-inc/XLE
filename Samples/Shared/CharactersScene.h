// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/Forward.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../PlatformRig/CameraManager.h"
#include <memory>

namespace EntityInterface { class  RetainedEntities; }

namespace Sample
{
    class AnimationNames;
    class CharacterInputFiles;

    class IPlayerCharacter : public RenderOverlays::DebuggingDisplay::IInputListener, public PlatformRig::Camera::ICameraAttach
    {
    public:
        virtual bool IsPresent() const = 0;
    };

    class CharactersScene
    {
    public:
        void Render(
            RenderCore::IThreadContext& context,
            SceneEngine::LightingParserContext& parserContext,
            int techniqueIndex) const;
        void Prepare(RenderCore::IThreadContext& context);
        void Cull(const Float4x4& worldToProjection);
        void Update(float deltaTime);

        void Clear();
        void CreateCharacter(
            uint64 id,
            const CharacterInputFiles&, const AnimationNames&,
            bool player, const Float4x4& localToWorld);
        void DeleteCharacter(uint64 id);

        std::shared_ptr<IPlayerCharacter> GetPlayerCharacter();

        Float4x4 DefaultCameraToWorld() const;

        CharactersScene();
        ~CharactersScene();

        class Pimpl;
    protected:
        std::shared_ptr<Pimpl> _pimpl;
    };

    void RegisterEntityInterface(
        EntityInterface::RetainedEntities& flexSys,
        const std::shared_ptr<CharactersScene>& sys);
}

