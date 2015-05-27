// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EntityInterface.h"
#include "../../Assets/AssetsCore.h"

namespace PlatformRig { class EnvironmentSettings; }
namespace Utility { class OutputStreamFormatter; }

namespace EntityInterface
{
    class RetainedEntities;
    class RetainedEntity;

    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const RetainedEntities& flexGobInterface,
            const RetainedEntity& obj);

    using EnvSettingsVector = std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>>;
    EnvSettingsVector BuildEnvironmentSettings(
        const RetainedEntities& flexGobInterface);

    void ExportEnvSettings(
        OutputStreamFormatter& formatter,
        const RetainedEntities& flexGobInterface,
        DocumentId docId);
}

