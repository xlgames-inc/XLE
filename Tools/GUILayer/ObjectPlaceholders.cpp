// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectPlaceholders.h"
#include "FlexGobInterface.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/VisualisationGeo.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"

namespace GUILayer
{
    using namespace RenderCore;

    class TechniqueMaterial
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        Techniques::TechniqueInterface _techniqueInterface;

        Techniques::ResolvedShader GetShaderType(
            Techniques::ParsingContext& parsingContext,
            unsigned techniqueIndex,
            const char shaderName[]);

        TechniqueMaterial(
            const Metal::InputLayout& inputLayout,
            const std::initializer_list<uint64>& objectCBs,
            ParameterBox materialParameters);
    };

    static Techniques::TechniqueInterface MakeTechInterface(
        const Metal::InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs)
    {
        Techniques::TechniqueInterface techniqueInterface(inputLayout);
        Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
        unsigned index = 0;
        for (auto o:objectCBs)
            techniqueInterface.BindConstantBuffer(o, index++, 1);
        return std::move(techniqueInterface);
    }

    static bool HasElement(const Metal::InputLayout& inputLayout, const char elementSemantic[])
    {
        auto end = &inputLayout.first[inputLayout.second];
        return std::find_if
            (
                inputLayout.first, end,
                [=](const Metal::InputElementDesc& element)
                    { return !XlCompareStringI(element._semanticName.c_str(), elementSemantic); }
            ) != end;
    }

    TechniqueMaterial::TechniqueMaterial(
        const Metal::InputLayout& inputLayout,
        const std::initializer_list<uint64>& objectCBs,
        ParameterBox materialParameters)
    : _materialParameters(std::move(materialParameters))
    , _techniqueInterface(MakeTechInterface(inputLayout, objectCBs))
    {
        if (HasElement(inputLayout, "NORMAL"))      _geometryParameters.SetParameter("GEO_HAS_NORMAL", 1);
        if (HasElement(inputLayout, "TEXCOORD"))    _geometryParameters.SetParameter("GEO_HAS_TEXCOORD", 1);
        if (HasElement(inputLayout, "TANGENT"))     _geometryParameters.SetParameter("GEO_HAS_TANGENT_FRAME", 1);
        if (HasElement(inputLayout, "COLOR"))       _geometryParameters.SetParameter("GEO_HAS_COLOUR", 1);
    }

    Techniques::ResolvedShader TechniqueMaterial::GetShaderType(
        Techniques::ParsingContext& parsingContext,
        unsigned techniqueIndex, const char shaderName[])
    {
        const ParameterBox* state[] = {
            &_geometryParameters, 
            &parsingContext.GetTechniqueContext()._globalEnvironmentState,
            &parsingContext.GetTechniqueContext()._runtimeState, 
            &_materialParameters
        };

        auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>(shaderName);
        return shaderType.FindVariation(techniqueIndex, state, _techniqueInterface);
    }

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

    namespace ObjectCBs
    {
        static const auto LocalTransform = Hash64("LocalTransform");
        static const auto BasicMaterialConstants = Hash64("BasicMaterialConstants");
    }
    
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

    class BasicMaterialConstants
    {
    public:
            // fixed set of material parameters currently.
        Float3 _materialDiffuse;    float _opacity;
        Float3 _materialSpecular;   float _alphaThreshold;
    };

    static void DrawObject(
        Metal::DeviceContext& devContext,
        Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        const VisGeoBox& visBox,
        const Techniques::ResolvedShader& shader, const FlexObjectType::Object& obj)
    {
        Metal::ConstantBufferPacket pkts[] = { 
            Techniques::MakeLocalTransformPacket(Identity<Float4x4>(), ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
            MakeSharedPkt(BasicMaterialConstants())
        };

        devContext.Bind(*shader._shaderProgram);
        shader._boundUniforms->Apply(
            devContext, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(pkts, nullptr, dimof(pkts)));
        devContext.Bind(*shader._boundLayout);

        devContext.Bind(MakeResourceList(visBox._cubeVB), visBox._cubeVBStride, 0);
        devContext.Draw(visBox._cubeVBCount);
    }

    void ObjectPlaceholders::Render(
        RenderCore::Metal::DeviceContext& metalContext, 
        SceneEngine::LightingParserContext& parserContext,
        unsigned techniqueIndex)
    {
        auto& visBox = Techniques::FindCachedBoxDep<VisGeoBox>(VisGeoBox::Desc());
        auto shader = visBox._material.GetShaderType(parserContext, techniqueIndex, "game/xleres/illum.txt");
        if (!shader._shaderProgram) return;

        for (auto a=_annotations.cbegin(); a!=_annotations.cend(); ++a) {
            auto objects = _objects->FindObjectsOfType(a->_typeId);
            for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {
                DrawObject(metalContext, parserContext, techniqueIndex, visBox, shader, **o);
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

