// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectPlaceholders.h"
#include "FlexGobInterface.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../Math/Geometry.h"

namespace GUILayer
{
    using namespace RenderCore;
    using namespace RenderCore::Techniques;

    namespace Parameters
    {
        static const auto Transform = ParameterBox::MakeParameterNameHash("Transform");
        static const auto Visible = ParameterBox::MakeParameterNameHash("Visible");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    using EditorDynamicInterface::FlexObjectScene;

    class VisGeoBox
    {
    public:
        class Desc {};

        Metal::VertexBuffer     _cubeVB;
        unsigned                _cubeVBCount;
        unsigned                _cubeVBStride;
        TechniqueMaterial       _material;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _depVal; }
        VisGeoBox(const Desc&);
        ~VisGeoBox();
    protected:
        std::shared_ptr<Assets::DependencyValidation> _depVal;
    };
    
    VisGeoBox::VisGeoBox(const Desc&)
    : _material(
        ToolsRig::Vertex3D_InputLayout, 
        { ObjectCBs::LocalTransform, ObjectCBs::BasicMaterialConstants },
        ParameterBox())
    {
        auto cubeVertices = ToolsRig::BuildCube();
        _cubeVBCount = (unsigned)cubeVertices.size();
        _cubeVBStride = (unsigned)sizeof(decltype(cubeVertices)::value_type);
        _cubeVB = Metal::VertexBuffer(AsPointer(cubeVertices.cbegin()), _cubeVBCount * _cubeVBStride);
        _depVal = std::make_shared<Assets::DependencyValidation>();
    }

    VisGeoBox::~VisGeoBox() {}

    static void DrawObject(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
        const VisGeoBox& visBox,
        const ResolvedShader& shader, const FlexObjectScene::Object& obj)
    {
        if (!obj._properties.GetParameter(Parameters::Visible, true)) return;

        const auto& cbLayout = ::Assets::GetAssetDep<Techniques::PredefinedCBLayout>(
            "game/xleres/BasicMaterialConstants.txt");

        shader.Apply(devContext, parserContext,
            {
                MakeLocalTransformPacket(
                    Transpose(obj._properties.GetParameter(Parameters::Transform, Identity<Float4x4>())),
                    ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                cbLayout.BuildCBDataAsPkt(ParameterBox())
            });
        
        devContext.Bind(MakeResourceList(visBox._cubeVB), visBox._cubeVBStride, 0);
        devContext.Draw(visBox._cubeVBCount);
    }

    void ObjectPlaceholders::Render(
        Metal::DeviceContext& metalContext, 
       ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
        auto& visBox = FindCachedBoxDep<VisGeoBox>(VisGeoBox::Desc());
        auto shader = visBox._material.FindVariation(parserContext, techniqueIndex, "game/xleres/illum.txt");
        if (!shader._shaderProgram) return;

        for (auto a=_annotations.cbegin(); a!=_annotations.cend(); ++a) {
            auto objects = _objects->FindObjectsOfType(a->_typeId);
            for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {
                DrawObject(metalContext, parserContext, visBox, shader, **o);
            }
        }
    }

    void ObjectPlaceholders::AddAnnotation(EditorDynamicInterface::ObjectTypeId typeId)
    {
        Annotation newAnnotation;
        newAnnotation._typeId = typeId;
        _annotations.push_back(newAnnotation);
    }

    class ObjectPlaceholders::IntersectionTester : public SceneEngine::IIntersectionTester
    {
    public:
        Result FirstRayIntersection(
            const SceneEngine::IntersectionTestContext& context,
            std::pair<Float3, Float3> worldSpaceRay) const;

        void FrustumIntersection(
            std::vector<Result>& results,
            const SceneEngine::IntersectionTestContext& context,
            const Float4x4& worldToProjection) const;

        IntersectionTester(std::shared_ptr<ObjectPlaceholders> placeHolders);
        ~IntersectionTester();
    protected:
        std::shared_ptr<ObjectPlaceholders> _placeHolders;
    };

    auto ObjectPlaceholders::IntersectionTester::FirstRayIntersection(
        const SceneEngine::IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay) const -> Result
    {
        using namespace SceneEngine;

        for (auto a=_placeHolders->_annotations.cbegin(); a!=_placeHolders->_annotations.cend(); ++a) {
            auto objects = _placeHolders->_objects->FindObjectsOfType(a->_typeId);
            for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {

                auto transform = Transpose((*o)->_properties.GetParameter(Parameters::Transform, Identity<Float4x4>()));
                if (RayVsAABB(worldSpaceRay, transform, Float3(-1.f, -1.f, -1.f), Float3(1.f, 1.f, 1.f))) {
                    Result result;
                    result._type = IntersectionTestScene::Type::Extra;
                    result._worldSpaceCollision = worldSpaceRay.first;
                    result._distance = 0.f;
                    result._objectGuid = std::make_pair((*o)->_doc, (*o)->_id);
                    result._drawCallIndex = 0;
                    result._materialGuid =0;
                    return result;
                }
            }
        }

        return Result();
    }

    void ObjectPlaceholders::IntersectionTester::FrustumIntersection(
        std::vector<Result>& results,
        const SceneEngine::IntersectionTestContext& context,
        const Float4x4& worldToProjection) const
    {}

    ObjectPlaceholders::IntersectionTester::IntersectionTester(std::shared_ptr<ObjectPlaceholders> placeHolders)
    : _placeHolders(placeHolders)
    {}

    ObjectPlaceholders::IntersectionTester::~IntersectionTester() {}

    std::shared_ptr<SceneEngine::IIntersectionTester> ObjectPlaceholders::CreateIntersectionTester()
    {
        return std::make_shared<IntersectionTester>(shared_from_this());
    }

    ObjectPlaceholders::ObjectPlaceholders(std::shared_ptr<FlexObjectScene> objects)
    : _objects(std::move(objects))
    {}

    ObjectPlaceholders::~ObjectPlaceholders() {}

}

