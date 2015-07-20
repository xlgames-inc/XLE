// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EntityInterface.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/UTFUtils.h"

namespace SceneEngine { class VolumetricFogManager; class ShallowSurfaceManager; }
namespace PlatformRig { class EnvironmentSettings; }

namespace Utility 
{ 
    class OutputStreamFormatter; 
    template<typename CharType> class InputStreamFormatter;
}

namespace EntityInterface
{
    class RetainedEntities;
    class RetainedEntity;

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const RetainedEntities& flexGobInterface,
            const RetainedEntity& obj);

///////////////////////////////////////////////////////////////////////////////////////////////////

    using EnvSettingsVector = 
        std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>>;
    EnvSettingsVector BuildEnvironmentSettings(
        const RetainedEntities& flexGobInterface);

    void ExportEnvSettings(
        OutputStreamFormatter& formatter,
        const RetainedEntities& flexGobInterface,
        DocumentId docId);
    EnvSettingsVector DeserializeEnvSettings(InputStreamFormatter<utf8>& formatter);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void RegisterEnvironmentFlexObjects(RetainedEntities& flexSys);
    void RegisterVolumetricFogFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::VolumetricFogManager> manager);

    void RegisterShallowSurfaceFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::ShallowSurfaceManager> manager);
}

