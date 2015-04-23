// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"
#include "VisualisationGeo.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../Math/Transformations.h"

#include "../../RenderCore/DX11/Metal/IncludeDX11.h"
#include <d3d11shader.h>

namespace ToolsRig
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc  GetCameraDesc() const
        {
            RenderCore::Techniques::CameraDesc result;
            result._cameraToWorld = MakeCameraToWorld(
                Normalize(_settings->_focus - _settings->_position),
                Float3(0.f, 0.f, 1.f), _settings->_position);
            result._farClip = _settings->_farClip;
            result._nearClip = _settings->_nearClip;
            result._verticalFieldOfView = Deg2Rad(_settings->_verticalFieldOfView);
            result._temporaryMatrix = Identity<Float4x4>();
            return result;
        }

        unsigned GetShadowProjectionCount() const { return 0; }
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
            { return SceneEngine::ShadowProjectionDesc(); }

        unsigned                        GetLightCount() const { return 1; }
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const
        {
            static SceneEngine::LightDesc light = DefaultDominantLight();
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            return DefaultGlobalLightingDesc();
        }

        float GetTimeValue() const { return 0.f; }

        VisSceneParser(std::shared_ptr<VisCameraSettings> settings)
            : _settings(std::move(settings)) {}
        ~VisSceneParser() {}

    protected:
        std::shared_ptr<VisCameraSettings> _settings;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void DrawPreviewGeometry(
        RenderCore::Metal::DeviceContext* metalContext,
        MaterialVisSettings::GeometryType::Enum geoType,
        RenderCore::Metal::ShaderProgram& shaderProgram)
    {
        using namespace RenderCore;

            //
            //      Geometry
            //
            //          There are different types of preview geometry we can feed into the system
            //          Plane2D         -- this is just a flat 2D plane covering the whole surface
            //          GeodesicSphere  -- this is a basic 3D sphere, with normals and texture coordinates
            //

        if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

            Metal::BoundInputLayout boundVertexInputLayout(
                Vertex2D_InputLayout, shaderProgram);
            metalContext->Bind(boundVertexInputLayout);
        
            const Internal::Vertex2D    vertices[] = 
            {
                { Float2(-1.f, -1.f),  Float2(0.f, 0.f) },
                { Float2( 1.f, -1.f),  Float2(1.f, 0.f) },
                { Float2(-1.f,  1.f),  Float2(0.f, 1.f) },
                { Float2( 1.f,  1.f),  Float2(1.f, 1.f) }
            };

            Metal::VertexBuffer vertexBuffer(vertices, sizeof(vertices));
            metalContext->Bind(MakeResourceList(vertexBuffer), sizeof(Internal::Vertex2D), 0);
            metalContext->Bind(Metal::Topology::TriangleStrip);
            metalContext->Draw(dimof(vertices));

        } else {

            Metal::BoundInputLayout boundVertexInputLayout(
                Vertex3D_InputLayout, shaderProgram);
            metalContext->Bind(boundVertexInputLayout);
            
            Metal::VertexBuffer vertexBuffer;
            unsigned count = 0;

            if (geoType == MaterialVisSettings::GeometryType::Sphere) {
                auto sphereGeometry = BuildGeodesicSphere();
                vertexBuffer = Metal::VertexBuffer(AsPointer(sphereGeometry.begin()), sphereGeometry.size() * sizeof(Internal::Vertex3D));
                count = unsigned(sphereGeometry.size());
            } else if (geoType == MaterialVisSettings::GeometryType::Cube) {
                auto cubeGeometry = BuildCube();
                vertexBuffer = Metal::VertexBuffer(AsPointer(cubeGeometry.begin()), cubeGeometry.size() * sizeof(Internal::Vertex3D));
                count = unsigned(cubeGeometry.size());
            }
            metalContext->Bind(MakeResourceList(vertexBuffer), sizeof(Internal::Vertex3D), 0);
            metalContext->Bind(Metal::Topology::TriangleList);
            metalContext->Draw(count);

        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static bool PositionInputIs2D(ID3D::ShaderReflection& reflection)
    {
            //
            //      Try to find out if the "POSITION" entry is 2D (or 3d/4d)...
            //
        D3D11_SHADER_DESC shaderDesc;
        reflection.GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.InputParameters; ++c) {
            D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
            HRESULT hresult = reflection.GetInputParameterDesc(c, &paramDesc);
            if (SUCCEEDED(hresult)) {
                if (!XlCompareStringI(paramDesc.SemanticName, "POSITION") && paramDesc.SemanticIndex == 0) {
                    if ((paramDesc.Mask & (~3)) == 0) {
                        return true;
                    } else {
                        return false;
                    }
                }
            }
        }

        return false;
    }

    static size_t WriteSystemVariable(
        const char name[], 
        const MaterialVisObject::SystemConstants& constants, 
        UInt2 viewportDims,
        void* destination, void* destinationEnd)
    {
        size_t size = size_t(destinationEnd) - size_t(destination);
        if (!_stricmp(name, "SI_OutputDimensions") && size >= (sizeof(unsigned)*2)) {
            ((unsigned*)destination)[0] = viewportDims[0];
            ((unsigned*)destination)[1] = viewportDims[1];
            return sizeof(unsigned)*2;
        } else if (!_stricmp(name, "SI_NegativeLightDirection") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightNegativeDirection;
            return sizeof(Float3);
        } else if (!_stricmp(name, "SI_LightColor") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightColour;
            return sizeof(Float3);
        } else if (!_stricmp(name, "MaterialDiffuse") && size >= sizeof(Float3)) {
            *((Float3*)destination) = Float3(1.f, 1.f, 1.f);
            return sizeof(Float3);
        } else if (!_stricmp(name, "Opacity") && size >= sizeof(float)) {
            *((float*)destination) = 1.f;
            return sizeof(float);
        } else if (!_stricmp(name, "MaterialSpecular") && size >= sizeof(Float3)) {
            *((Float3*)destination) = Float3(1.f, 1.f, 1.f);
            return sizeof(Float3);
        } else if (!_stricmp(name, "AlphaThreshold") && size >= sizeof(float)) {
            *((float*)destination) = .33f;
            return sizeof(float);
        }
        return 0;
    }

    static ImpliedTyping::TypeDesc GetType(D3D11_SHADER_TYPE_DESC typeDesc)
    {
        using namespace Utility::ImpliedTyping;
        TypeDesc result;
        switch (typeDesc.Type) {
        case D3D_SVT_BOOL:  result._type = TypeCat::Bool;   break;
        case D3D_SVT_INT:   result._type = TypeCat::Int32;  break;
        case D3D_SVT_FLOAT: result._type = TypeCat::Float;  break;
        case D3D_SVT_UINT:
        case D3D_SVT_UINT8: result._type = TypeCat::UInt32; break;

        default:
        case D3D_SVT_VOID:  result._type = TypeCat::Void;   break;
        }

        if (typeDesc.Elements > 0) {
            result._arrayCount = uint16(typeDesc.Elements);
        } else if (typeDesc.Class == D3D_SVC_VECTOR) {
            result._arrayCount = uint16(typeDesc.Columns);
        } else if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS) {
            result._arrayCount = uint16(typeDesc.Columns * typeDesc.Rows);
        }

        return result;
    }

    static std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>> 
        BuildMaterialConstants(
            ID3D::ShaderReflection& reflection, 
            const ParameterBox& constants,
            const MaterialVisObject::SystemConstants& systemConstantsContext,
            UInt2 viewportDims)
    {

            //
            //      Find the cbuffers, and look for the variables
            //      within. Attempt to fill those values with the appropriate values
            //      from the current previewing material state
            //
        std::vector<std::pair<uint64, RenderCore::Metal::ConstantBufferPacket>> finalResult;

        D3D11_SHADER_DESC shaderDesc;
        reflection.GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection.GetResourceBindingDesc(c, &bindDesc);

            if (bindDesc.Type == D3D10_SIT_CBUFFER) {
                auto cbuffer = reflection.GetConstantBufferByName(bindDesc.Name);
                if (cbuffer) {
                    D3D11_SHADER_BUFFER_DESC bufferDesc;
                    HRESULT hresult = cbuffer->GetDesc(&bufferDesc);
                    if (SUCCEEDED(hresult)) {
                        RenderCore::SharedPkt result;

                        for (unsigned c=0; c<bufferDesc.Variables; ++c) {
                            auto reflectionVariable = cbuffer->GetVariableByIndex(c);
                            D3D11_SHADER_VARIABLE_DESC variableDesc;
                            hresult = reflectionVariable->GetDesc(&variableDesc);
                            if (SUCCEEDED(hresult)) {

                                    //
                                    //      If the variable is within our table of 
                                    //      material parameter values, then copy that
                                    //      value into the appropriate place in the cbuffer.
                                    //
                                    //      However, note that this may require a cast sometimes
                                    //

                                auto nameHash = ParameterBox::MakeParameterNameHash(variableDesc.Name);
                                auto hasParam = constants.HasParameter(nameHash);
                                if (hasParam) {

                                    auto type = reflectionVariable->GetType();
                                    D3D11_SHADER_TYPE_DESC typeDesc;
                                    hresult = type->GetDesc(&typeDesc);
                                    if (SUCCEEDED(hresult)) {

                                            //
                                            //      Finally, copy whatever the material object
                                            //      is, into the destination position in the 
                                            //      constant buffer;
                                            //

                                        auto impliedType = GetType(typeDesc);
                                        assert((variableDesc.StartOffset + impliedType.GetSize()) <= bufferDesc.Size);
                                        if ((variableDesc.StartOffset + impliedType.GetSize()) <= bufferDesc.Size) {

                                            if (!result.size()) {
                                                result = RenderCore::MakeSharedPktSize(bufferDesc.Size);
                                                std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                                            }

                                            constants.GetParameter(
                                                nameHash,
                                                PtrAdd(result.begin(), variableDesc.StartOffset),
                                                impliedType);
                                        }
                                    }

                                } else {
                                    
                                    if (!result.size()) {
                                        char buffer[4096];
                                        if (size_t size = WriteSystemVariable(
                                            variableDesc.Name, systemConstantsContext, viewportDims,
                                            buffer, PtrAdd(buffer, std::min(sizeof(buffer), (size_t)(bufferDesc.Size - variableDesc.StartOffset))))) {

                                            result = RenderCore::MakeSharedPktSize(bufferDesc.Size);
                                            std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                                            XlCopyMemory(PtrAdd(result.begin(), variableDesc.StartOffset), buffer, size);
                                        }
                                    } else {
                                        WriteSystemVariable(
                                            variableDesc.Name, systemConstantsContext, viewportDims,
                                            PtrAdd(result.begin(), variableDesc.StartOffset), result.end());
                                    }
                                }

                            }
                        }

                        if (result.size()) {
                            finalResult.push_back(
                                std::make_pair(Hash64(bindDesc.Name), std::move(result)));
                        }   
                    }
                }
            }

        }

        return finalResult;
    }

    static std::vector<const RenderCore::Metal::ShaderResourceView*>
        BuildBoundTextures(
            RenderCore::Metal::BoundUniforms& boundUniforms,
            RenderCore::Metal::ShaderProgram& shaderProgram,
            const ParameterBox& bindings,
            const Assets::DirectorySearchRules& searchRules)
    {
        using namespace RenderCore;
        std::vector<const Metal::ShaderResourceView*> result;
        std::vector<uint64> alreadyBound;

            //
            //      For each entry in our resource binding set, we're going
            //      to register a binding in the BoundUniforms, and find
            //      the associated shader resource view.
            //      For any shader resources that are used by the shader, but
            //      not bound to anything -- we need to assign them to the 
            //      default objects.
            //

        const Metal::CompiledShaderByteCode* shaderCode[] = {
            &shaderProgram.GetCompiledVertexShader(),
            &shaderProgram.GetCompiledPixelShader(),
            shaderProgram.GetCompiledGeometryShader(),
        };

        for (unsigned s=0; s<dimof(shaderCode); ++s) {
            if (!shaderCode[s]) continue;

            auto reflection = shaderCode[s]->GetReflection();
            D3D11_SHADER_DESC shaderDesc;
            reflection->GetDesc(&shaderDesc);

            for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

                D3D11_SHADER_INPUT_BIND_DESC bindDesc;
                reflection->GetResourceBindingDesc(c, &bindDesc);
                if  (bindDesc.Type == D3D10_SIT_TEXTURE) {

                    auto str = bindings.GetString<::Assets::ResChar>(ParameterBox::MakeParameterNameHash(bindDesc.Name));
                    if (str.empty()) {
                            //  It's not mentioned in the material resources. try to look
                            //  for a default resource for this bind point
                        str = ::Assets::rstring("game/xleres/DefaultResources/") + bindDesc.Name + ".dds";
                    }

                    auto bindingHash = Hash64(bindDesc.Name, &bindDesc.Name[XlStringLen(bindDesc.Name)]);
                    if (std::find(alreadyBound.cbegin(), alreadyBound.cend(), bindingHash) != alreadyBound.cend()) {
                        continue;
                    }
                    alreadyBound.push_back(bindingHash);

                    if (!str.empty()) {
                        TRY {
                            ResChar resolvedFile[MaxPath];
                            searchRules.ResolveFile(
                                resolvedFile, dimof(resolvedFile),
                                str.c_str());

                            const Metal::DeferredShaderResource& texture = 
                                ::Assets::GetAssetDep<Metal::DeferredShaderResource>(resolvedFile);

                            result.push_back(&texture.GetShaderResource());
                            boundUniforms.BindShaderResource(
                                bindingHash,
                                unsigned(result.size()-1), 1);
                        }
                        CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                        CATCH_END
                    }
                
                } else if (bindDesc.Type == D3D10_SIT_SAMPLER) {

                        //  we should also bind samplers to something
                        //  reasonable, also...

                }
            }
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialSceneParser : public VisSceneParser
    {
    public:
        void ExecuteScene(  RenderCore::Metal::DeviceContext* context, 
                            SceneEngine::LightingParserContext& parserContext, 
                            const SceneEngine::SceneParseSettings& parseSettings,
                            unsigned techniqueIndex) const 
        {
            if (    parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::Depth
                ||  parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::General) {

                    // draw here
                Draw(context, parserContext);
            }
        }

        void ExecuteShadowScene(    RenderCore::Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
        }

        void Draw(  RenderCore::Metal::DeviceContext* metalContext, 
                    SceneEngine::LightingParserContext& parserContext) const
        {
            using namespace RenderCore;
            metalContext->Bind(*_object->_shaderProgram);
            metalContext->Bind(RenderCore::Techniques::CommonResources()._defaultRasterizer);

                // disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext->Bind(RenderCore::Techniques::CommonResources()._blendOpaque);

                //
                //      Constants / Resources
                //

            Metal::ViewportDesc currentViewport(*metalContext);
            
            auto materialConstants = BuildMaterialConstants(
                *_object->_shaderProgram->GetCompiledPixelShader().GetReflection(), 
                _object->_parameters._constants,
                _object->_systemConstants, UInt2(unsigned(currentViewport.Width), unsigned(currentViewport.Height)));
        
            Metal::BoundUniforms boundLayout(*_object->_shaderProgram);
            Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            
            std::vector<RenderCore::Metal::ConstantBufferPacket> constantBufferPackets;
            constantBufferPackets.push_back(
                Techniques::MakeLocalTransformPacket(Identity<Float4x4>(), GetCameraDesc()));
            boundLayout.BindConstantBuffer(Techniques::ObjectCBs::LocalTransform, 0, 1);
            for (auto i=materialConstants.cbegin(); i!=materialConstants.cend(); ++i) {
                boundLayout.BindConstantBuffer(i->first, unsigned(constantBufferPackets.size()), 1);
                constantBufferPackets.push_back(std::move(i->second));
            }

            auto boundTextures = BuildBoundTextures(
                boundLayout, *_object->_shaderProgram,
                _object->_parameters._bindings, _object->_searchRules);
            boundLayout.Apply(
                *metalContext,
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream( 
                    AsPointer(constantBufferPackets.begin()), nullptr, constantBufferPackets.size(), 
                    AsPointer(boundTextures.begin()), boundTextures.size()));

            auto geoType = _settings->_geometryType;
            if (PositionInputIs2D(*_object->_shaderProgram->GetCompiledVertexShader().GetReflection())) {
                geoType = MaterialVisSettings::GeometryType::Plane2D;
            }

            DrawPreviewGeometry(metalContext, geoType, *_object->_shaderProgram);
        }

        MaterialSceneParser(
            const MaterialVisSettings& settings,
            const MaterialVisObject& object)
        : VisSceneParser(settings._camera), _settings(&settings), _object(&object) {}

    protected:
        const MaterialVisSettings*  _settings;
        const MaterialVisObject*    _object;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool MaterialVisLayer::Draw(
        RenderCore::IThreadContext& context,
        SceneEngine::LightingParserContext& parserContext,
        const MaterialVisSettings& settings,
        const MaterialVisObject& object)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;

        TRY {

            MaterialSceneParser sceneParser(settings, object);
            SceneEngine::RenderingQualitySettings qualSettings(context.GetStateDesc()._viewportDimensions);

            if (settings._lightingType == MaterialVisSettings::LightingType::NoLightingParser) {
                
                auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
                SceneEngine::LightingParser_SetupScene(
                    metalContext.get(), parserContext, 
                    &sceneParser, sceneParser.GetCameraDesc(),
                    qualSettings);
                sceneParser.Draw(metalContext.get(), parserContext);
                
            } else if (settings._lightingType == MaterialVisSettings::LightingType::Deferred) {
                qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Deferred;
                SceneEngine::LightingParser_ExecuteScene(context, parserContext, sceneParser, qualSettings);
            } else if (settings._lightingType == MaterialVisSettings::LightingType::Forward) {
                qualSettings._lightingModel = SceneEngine::RenderingQualitySettings::LightingModel::Forward;
                SceneEngine::LightingParser_ExecuteScene(context, parserContext, sceneParser, qualSettings);
            }

            return true;
        }
        CATCH (::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH (::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH_END

        return false;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialVisLayer::Pimpl
    {
    public:
        std::shared_ptr<MaterialVisSettings> _settings;
        std::shared_ptr<MaterialVisObject> _object;
    };

    auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
        { return nullptr; }

    void MaterialVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        assert(context);
        Draw(*context, parserContext, *_pimpl->_settings, *_pimpl->_object);
    }

    void MaterialVisLayer::RenderWidgets(
        RenderCore::IThreadContext* context, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {}

    void MaterialVisLayer::SetActivationState(bool) {}

    MaterialVisLayer::MaterialVisLayer(
        std::shared_ptr<MaterialVisSettings> settings,
        std::shared_ptr<MaterialVisObject> object)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = settings;
        _pimpl->_object = object;
    }

    MaterialVisLayer::~MaterialVisLayer()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    MaterialVisSettings::MaterialVisSettings()
    {
        _geometryType = GeometryType::Sphere;
        _lightingType = LightingType::NoLightingParser;
        _camera = std::make_shared<VisCameraSettings>();
        _camera->_position = Float3(-5.f, 0.f, 0.f);
    }

    MaterialVisObject::SystemConstants::SystemConstants()
    {
        _lightNegativeDirection = Float3(0.f, 0.f, 1.f);
        _lightColour = Float3(1.f, 1.f, 1.f);
    }

}

