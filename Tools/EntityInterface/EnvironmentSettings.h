// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FlexGobInterface.h"

namespace PlatformRig { class EnvironmentSettings; }

namespace GUILayer
{
    PlatformRig::EnvironmentSettings
        BuildEnvironmentSettings(
            const EditorDynamicInterface::FlexObjectScene& flexGobInterface,
            const EditorDynamicInterface::FlexObjectScene::Object& obj);

    using EnvSettingsVector = std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>>;
    EnvSettingsVector BuildEnvironmentSettings(
        const EditorDynamicInterface::FlexObjectScene& flexGobInterface);

    void ExportEnvSettings(
        const EditorDynamicInterface::FlexObjectScene& flexGobInterface,
        EditorDynamicInterface::DocumentId docId,
        const ::Assets::ResChar destinationFile[]);
}

