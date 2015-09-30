// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ManipulatorUtils.h"
#include "DelayedDeleteQueue.h"
#include "CLIXAutoPtr.h"
#include <string>
#include <memory>
#include <vector>

namespace SceneEngine { class TerrainManager; class PlacementsEditor; }
namespace ToolsRig { class IPlacementManipulatorSettings; }

namespace GUILayer
{
    class TerrainManipulatorsPimpl
    {
    public:
        class RegisteredManipulator
		{
		public:
			std::string _name;
			std::shared_ptr<ToolsRig::IManipulator> _manipulator;
			RegisteredManipulator(
				const std::string& name,
				std::shared_ptr<ToolsRig::IManipulator> manipulator)
				: _name(name), _manipulator(std::move(manipulator))
			{}
			RegisteredManipulator() {}
			~RegisteredManipulator();
		};
		std::vector<RegisteredManipulator> _terrainManipulators;
    };

    ref class TerrainManipulators : public IManipulatorSet
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) override;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() override;

        TerrainManipulators(std::shared_ptr<SceneEngine::TerrainManager> terrain);
        ~TerrainManipulators();
    protected:
        clix::auto_ptr<TerrainManipulatorsPimpl> _pimpl;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlacementManipulatorsPimpl
    {
    public:
        class RegisteredManipulator
		{
		public:
			std::string _name;
			std::shared_ptr<ToolsRig::IManipulator> _manipulator;
			RegisteredManipulator(
				const std::string& name,
				std::shared_ptr<ToolsRig::IManipulator> manipulator)
				: _name(name), _manipulator(std::move(manipulator))
			{}
			RegisteredManipulator() {}
			~RegisteredManipulator();
		};
		std::vector<RegisteredManipulator> _manipulators;
    };

    ref class PlacementManipulators : public IManipulatorSet
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) override;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() override;

        PlacementManipulators(
            ToolsRig::IPlacementManipulatorSettings* context,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlacementManipulators();
    protected:
        clix::auto_ptr<PlacementManipulatorsPimpl> _pimpl;
    };

    public ref class IPlacementManipulatorSettingsLayer abstract
    {
    public:
        virtual String^ GetSelectedModel() = 0;
        virtual String^ GetSelectedMaterial() = 0;
        virtual void EnableSelectedModelDisplay(bool newState) = 0;
        virtual void SelectModel(String^ newModelName, String^ newMaterialName) = 0;
        virtual void SwitchToMode(unsigned newMode) = 0;

        ToolsRig::IPlacementManipulatorSettings* GetNative();
        
        IPlacementManipulatorSettingsLayer();
        virtual ~IPlacementManipulatorSettingsLayer();
    private:
        clix::auto_ptr<ToolsRig::IPlacementManipulatorSettings> _native;
    };

}

