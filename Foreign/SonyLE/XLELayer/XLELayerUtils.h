// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Tools/ToolsRig/VisualisationUtils.h"
#include "../../Math/Vector.h"

namespace XLELayer
{
    static Float3 AsFloat3(Sce::Atf::VectorMath::Vec3F^ input) { return Float3(input->X, input->Y, input->Z); }

    public ref class XLELayerUtils
    {
    public:
        static RenderCore::Techniques::CameraDesc AsCameraDesc(Sce::Atf::Rendering::Camera^ camera)
        {
            ToolsRig::VisCameraSettings visCam;
            visCam._position = AsFloat3(camera->WorldEye);
            visCam._focus = AsFloat3(camera->WorldLookAtPoint);
            visCam._verticalFieldOfView = camera->YFov * 180.f / gPI;
            visCam._nearClip = camera->NearZ;
            visCam._farClip = camera->FarZ;
            return RenderCore::Techniques::CameraDesc(ToolsRig::AsCameraDesc(visCam));
        }

        static GUILayer::IntersectionTestContextWrapper^
            CreateIntersectionTestContext(
                GUILayer::EngineDevice^ engineDevice,
                GUILayer::TechniqueContextWrapper^ techniqueContext,
                Sce::Atf::Rendering::Camera^ camera)
        {
            return GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
                engineDevice, techniqueContext, AsCameraDesc(camera));
        }
    };
}

