// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DelayedDeleteQueue.h"
#include "CLIXAutoPtr.h"
#include "../../Core/Types.h"
#include <vector>
#include <utility>

namespace SceneEngine { class PlacementsEditor; }
namespace PlatformRig { class EnvironmentSettings; }

using namespace System;
using namespace System::Collections::Generic;

namespace GUILayer
{
    ref class PlacementsEditorWrapper;

    public ref class ObjectSet
    {
    public:
        void Add(uint64 document, uint64 id);
        void Clear();
        bool IsEmpty();
        void DoFixup(SceneEngine::PlacementsEditor& placements);
        void DoFixup(PlacementsEditorWrapper^ placements);

        typedef std::vector<std::pair<uint64, uint64>> NativePlacementSet;
		clix::auto_ptr<NativePlacementSet> _nativePlacements;

        ObjectSet();
        ~ObjectSet();
    };

    using EnvSettingsVector = std::vector<std::pair<std::string, PlatformRig::EnvironmentSettings>>;

    ref class EditorSceneManager;

    public ref class EnvironmentSettingsSet
    {
    public:
        clix::auto_ptr<EnvSettingsVector> _settings;
        property IEnumerable<String^>^ Names { IEnumerable<String^>^ get(); }

        void AddDefault();
        const PlatformRig::EnvironmentSettings& GetSettings(String^ name);

        EnvironmentSettingsSet(EditorSceneManager^ scene);
        ~EnvironmentSettingsSet();
    };
}