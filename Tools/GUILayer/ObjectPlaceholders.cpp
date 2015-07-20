// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectPlaceholders.h"
#include "ExportedNativeTypes.h"
#include "../EntityInterface/RetainedEntities.h"
#include "../ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../Math/Geometry.h"
#include "../../Utility/StringUtils.h"

namespace GUILayer
{
    using namespace RenderCore;
    using namespace RenderCore::Techniques;

    namespace Parameters
    {
        static const auto Transform = ParameterBox::MakeParameterNameHash("Transform");
        static const auto Translation = ParameterBox::MakeParameterNameHash("Translation");
        static const auto Visible = ParameterBox::MakeParameterNameHash("Visible");
        static const auto ShowMarker = ParameterBox::MakeParameterNameHash("ShowMarker");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    using EntityInterface::RetainedEntities;
    using EntityInterface::RetainedEntity;

    class VisGeoBox
    {
    public:
        class Desc {};

        Metal::VertexBuffer     _cubeVB;
        unsigned                _cubeVBCount;
        unsigned                _cubeVBStride;
        TechniqueMaterial       _material;
        TechniqueMaterial       _materialP;

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
    , _materialP(
        Metal::GlobalInputLayouts::P,
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

    static Float4x4 GetTransform(const RetainedEntity& obj)
    {
        auto xform = obj._properties.GetParameter<Float4x4>(Parameters::Transform);
        if (xform.first) return Transpose(xform.second);

        auto transl = obj._properties.GetParameter<Float3>(Parameters::Translation);
        if (transl.first) {
            return AsFloat4x4(transl.second);
        }
        return Identity<Float4x4>();
    }

    static bool GetShowMarker(const RetainedEntity& obj)
    {
        return obj._properties.GetParameter(Parameters::ShowMarker, true);
    }

    static void DrawObject(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
        const VisGeoBox& visBox,
        const ResolvedShader& shader, const RetainedEntity& obj)
    {
        if (!obj._properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(obj)) return;

        const auto& cbLayout = ::Assets::GetAssetDep<Techniques::PredefinedCBLayout>(
            "game/xleres/BasicMaterialConstants.txt");

        shader.Apply(devContext, parserContext,
            {
                MakeLocalTransformPacket(
                    GetTransform(obj),
                    ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                cbLayout.BuildCBDataAsPkt(ParameterBox())
            });
        
        devContext.Bind(MakeResourceList(visBox._cubeVB), visBox._cubeVBStride, 0);
        devContext.Draw(visBox._cubeVBCount);
    }

    static void DrawTriMeshMarker(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
        const VisGeoBox& visBox,
        const ResolvedShader& shader, const RetainedEntity& obj,
        EntityInterface::RetainedEntities& objs)
    {
        static auto IndexListHash = ParameterBox::MakeParameterNameHash("IndexList");

        if (!obj._properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(obj)) return;

        // we need an index list with at least 3 indices (to make at least one triangle)
        auto indexListType = obj._properties.GetParameterType(IndexListHash);
        if (indexListType._type == ImpliedTyping::TypeCat::Void || indexListType._arrayCount < 3)
            return;

        auto ibData = std::make_unique<unsigned[]>(indexListType._arrayCount);
        bool success = obj._properties.GetParameter(
            IndexListHash, ibData.get(), 
            ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::UInt32, indexListType._arrayCount));
        if (!success) return;

        const auto& chld = obj._children;
        if (!chld.size()) return;

        auto vbData = std::make_unique<Float3[]>(chld.size());
        for (size_t c=0; c<chld.size(); ++c) {
            const auto* e = objs.GetEntity(obj._doc, chld[c]);
            if (e) {
                vbData[c] = ExtractTranslation(GetTransform(*e));
            } else {
                vbData[c] = Zero<Float3>();
            }
        }
        
        const auto& cbLayout = ::Assets::GetAssetDep<Techniques::PredefinedCBLayout>(
            "game/xleres/BasicMaterialConstants.txt");

        shader.Apply(devContext, parserContext,
            {
                MakeLocalTransformPacket(
                    GetTransform(obj),
                    ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                cbLayout.BuildCBDataAsPkt(ParameterBox())
            });

        devContext.Bind(Techniques::CommonResources()._blendAdditive);
        devContext.Bind(Techniques::CommonResources()._dssReadOnly);
        devContext.Bind(Techniques::CommonResources()._cullDisable);
        
        Metal::VertexBuffer vb(vbData.get(), sizeof(Float3)*chld.size());
        Metal::IndexBuffer ib(ibData.get(), sizeof(unsigned)*indexListType._arrayCount);

        devContext.Bind(MakeResourceList(vb), sizeof(Float3), 0);
        devContext.Bind(ib, Metal::NativeFormat::R32_UINT);
        devContext.Bind(Metal::Topology::TriangleList);
        devContext.DrawIndexed(indexListType._arrayCount);
    }

    void ObjectPlaceholders::Render(
        Metal::DeviceContext& metalContext, 
        ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
        auto& visBox = FindCachedBoxDep<VisGeoBox>(VisGeoBox::Desc());
        auto shader = visBox._material.FindVariation(parserContext, techniqueIndex, "game/xleres/illum.txt");
        if (shader._shaderProgram) {
            for (auto a=_cubeAnnotations.cbegin(); a!=_cubeAnnotations.cend(); ++a) {
                auto objects = _objects->FindEntitiesOfType(a->_typeId);
                for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {
                    DrawObject(metalContext, parserContext, visBox, shader, **o);
                }
            }
        }

        auto shaderP = visBox._materialP.FindVariation(parserContext, techniqueIndex, "game/xleres/techniques/meshmarker.txt");
        if (shaderP._shaderProgram) {
            for (const auto&a:_triMeshAnnotations) {
                auto objects = _objects->FindEntitiesOfType(a._typeId);
                for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {
                    DrawTriMeshMarker(metalContext, parserContext, visBox, shaderP, **o, *_objects);
                }
            }
        }
    }

    void ObjectPlaceholders::AddAnnotation(EntityInterface::ObjectTypeId typeId, const std::string& geoType)
    {
        Annotation newAnnotation;
        newAnnotation._typeId = typeId;

        if (XlEqStringI(geoType, "TriMeshMarker")) {
            _triMeshAnnotations.push_back(newAnnotation);
        } else {
            _cubeAnnotations.push_back(newAnnotation);
        }
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

        for (auto a=_placeHolders->_cubeAnnotations.cbegin(); a!=_placeHolders->_cubeAnnotations.cend(); ++a) {
            auto objects = _placeHolders->_objects->FindEntitiesOfType(a->_typeId);
            for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {

                auto transform = GetTransform(**o);
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

    ObjectPlaceholders::ObjectPlaceholders(std::shared_ptr<RetainedEntities> objects)
    : _objects(std::move(objects))
    {}

    ObjectPlaceholders::~ObjectPlaceholders() {}

}

