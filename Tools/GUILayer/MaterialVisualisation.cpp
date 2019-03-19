// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UITypesBinding.h"
#include "ExportedNativeTypes.h"
#include "EditorInterfaceUtils.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../ToolsRig/MaterialVisualisation.h"
#include "../../RenderCore/Assets/RawMaterial.h"

#pragma warning(disable:4505)		// 'GUILayer::AsNative': unreferenced local function has been removed)

namespace GUILayer
{
	static ToolsRig::VisEnvSettings::LightingType AsNative(
         MaterialVisSettings::LightingType input)
    {
        switch (input) {
        case MaterialVisSettings::LightingType::Deferred:
            return ToolsRig::VisEnvSettings::LightingType::Deferred;
        case MaterialVisSettings::LightingType::Forward:
            return ToolsRig::VisEnvSettings::LightingType::Forward;
        default:
            return ToolsRig::VisEnvSettings::LightingType::Direct;
        }
    }

	RenderCore::Assets::MaterialScaffoldMaterial ResolveNativeMaterial(
		System::Collections::Generic::IEnumerable<RawMaterial^>^ rawMaterials,
		const ::Assets::DirectorySearchRules& searchRules)
	{
		RenderCore::Assets::MaterialScaffoldMaterial result;
		for each(auto c in rawMaterials)
			RenderCore::Assets::MergeIn_Stall(result, *c->GetUnderlying(), searchRules);
		return result;
	}
    
    static MaterialVisSettings::GeometryType AsManaged(
        ToolsRig::MaterialVisSettings::GeometryType input)
    {
        switch (input) {
        case ToolsRig::MaterialVisSettings::GeometryType::Sphere:
            return MaterialVisSettings::GeometryType::Sphere;
        case ToolsRig::MaterialVisSettings::GeometryType::Cube:
            return MaterialVisSettings::GeometryType::Cube;
        default:
            return MaterialVisSettings::GeometryType::Plane2D;
        }
    }

    static ToolsRig::MaterialVisSettings::GeometryType AsNative(
         MaterialVisSettings::GeometryType input)
    {
        switch (input) {
        case MaterialVisSettings::GeometryType::Sphere:
            return ToolsRig::MaterialVisSettings::GeometryType::Sphere;
        case MaterialVisSettings::GeometryType::Cube:
            return ToolsRig::MaterialVisSettings::GeometryType::Cube;
        default:
            return ToolsRig::MaterialVisSettings::GeometryType::Plane2D;
        }
    }

	MaterialVisSettings::MaterialVisSettings()
	{
		Geometry = GeometryType::Sphere;
		Lighting = LightingType::Deferred;
	}

	std::shared_ptr<ToolsRig::MaterialVisSettings> MaterialVisSettings::ConvertToNative()
	{
		auto result = std::make_shared<ToolsRig::MaterialVisSettings>();
		result->_geometryType = AsNative(Geometry);
		return result;
	}

	MaterialVisSettings^ MaterialVisSettings::ConvertFromNative(const ToolsRig::MaterialVisSettings& input)
	{
		MaterialVisSettings^ result = gcnew MaterialVisSettings();
		result->Geometry = AsManaged(input._geometryType);
		return result;
	}

}
