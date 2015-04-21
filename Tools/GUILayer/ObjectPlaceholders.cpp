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
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"

namespace GUILayer
{
    using namespace RenderCore;
    using namespace RenderCore::Techniques;

///////////////////////////////////////////////////////////////////////////////////////////////////

    using FlexObjectType = EditorDynamicInterface::FlexObjectType;

    class VisGeoBox
    {
    public:
        class Desc {};

        Metal::VertexBuffer     _cubeVB;
        unsigned                _cubeVBCount;
        unsigned                _cubeVBStride;
        TechniqueMaterial       _material;

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_depVal; }
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
        const ResolvedShader& shader, const FlexObjectType::Object& obj)
    {
        shader.Apply(devContext, parserContext,
            {
                MakeLocalTransformPacket(Identity<Float4x4>(), ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                MakeSharedPkt(BasicMaterialConstants())
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

    ObjectPlaceholders::ObjectPlaceholders(std::shared_ptr<FlexObjectType> objects)
    : _objects(std::move(objects))
    {}

    ObjectPlaceholders::~ObjectPlaceholders() {}

}

