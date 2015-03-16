// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/LightDesc.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/IThreadContext.h"
#include "../Math/Transformations.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#include <d3d11shader.h>

namespace PlatformRig
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
            static SceneEngine::LightDesc light;
            light._type = SceneEngine::LightDesc::Directional;
            light._lightColour = 5.f * Float3(5.f, 5.f, 5.f);
            light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
            light._radius = 10000.f;
            light._shadowFrustumIndex = ~unsigned(0x0);
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            SceneEngine::GlobalLightingDesc result;
            result._ambientLight = 5.f * Float3(0.25f, 0.25f, 0.25f);
            result._skyTexture = nullptr;
            result._doAtmosphereBlur = false;
            result._doOcean = false;
            result._doToneMap = false;
            return result;
        }

        float GetTimeValue() const { return 0.f; }

        VisSceneParser(std::shared_ptr<VisCameraSettings> settings)
            : _settings(std::move(settings)) {}
        ~VisSceneParser() {}

    protected:
        std::shared_ptr<VisCameraSettings> _settings;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        #pragma pack(push)
        #pragma pack(1)
        class Vertex2D
        {
        public:
            Float2      _position;
            Float2      _texCoord;
        };

        class Vertex3D
        {
        public:
            Float3      _position;
            Float3      _normal;
            Float2      _texCoord;
        };
        #pragma pack(pop)

        static RenderCore::Metal::InputElementDesc Vertex2D_InputLayout[] = {
            RenderCore::Metal::InputElementDesc( "POSITION", 0, RenderCore::Metal::NativeFormat::R32G32_FLOAT ),
            RenderCore::Metal::InputElementDesc( "TEXCOORD", 0, RenderCore::Metal::NativeFormat::R32G32_FLOAT )
        };

        static RenderCore::Metal::InputElementDesc Vertex3D_InputLayout[] = {
            RenderCore::Metal::InputElementDesc( "POSITION", 0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
            RenderCore::Metal::InputElementDesc(   "NORMAL", 0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
            RenderCore::Metal::InputElementDesc( "TEXCOORD", 0, RenderCore::Metal::NativeFormat::R32G32_FLOAT )
        };
    }

    static void GeodesicSphere_Subdivide(const Float3 &v1, const Float3 &v2, const Float3 &v3, std::vector<Float3> &sphere_points, unsigned int depth) 
    {
        if(depth == 0) 
        {
            sphere_points.push_back(v1);
            sphere_points.push_back(v2);
            sphere_points.push_back(v3);
            return;
        }

        Float3 v12 = Normalize(v1 + v2);
        Float3 v23 = Normalize(v2 + v3);
        Float3 v31 = Normalize(v3 + v1);
        GeodesicSphere_Subdivide( v1, v12, v31, sphere_points, depth - 1);
        GeodesicSphere_Subdivide( v2, v23, v12, sphere_points, depth - 1);
        GeodesicSphere_Subdivide( v3, v31, v23, sphere_points, depth - 1);
        GeodesicSphere_Subdivide(v12, v23, v31, sphere_points, depth - 1);
    }

    static std::vector<Float3>     BuildGeodesicSpherePts(int detail = 4)
    {

            //  
            //      Basic geodesic sphere generation code
            //          Based on a document from http://www.opengl.org.ru/docs/pg/0208.html
            //
        const float X = 0.525731112119133606f;
        const float Z = 0.850650808352039932f;
        const Float3 vdata[12] = 
        {
            Float3(  -X, 0.0,   Z ), Float3(   X, 0.0,   Z ), Float3(  -X, 0.0,  -Z ), Float3(   X, 0.0,  -Z ),
            Float3( 0.0,   Z,   X ), Float3( 0.0,   Z,  -X ), Float3( 0.0,  -Z,   X ), Float3( 0.0,  -Z,  -X ),
            Float3(   Z,   X, 0.0 ), Float3(  -Z,   X, 0.0 ), Float3(   Z,  -X, 0.0 ), Float3(  -Z,  -X, 0.0 )
        };

        int tindices[20][3] = 
        {
            { 0,  4,  1 }, { 0, 9,  4 }, { 9,  5, 4 }, {  4, 5, 8 }, { 4, 8,  1 },
            { 8, 10,  1 }, { 8, 3, 10 }, { 5,  3, 8 }, {  5, 2, 3 }, { 2, 7,  3 },
            { 7, 10,  3 }, { 7, 6, 10 }, { 7, 11, 6 }, { 11, 0, 6 }, { 0, 1,  6 },
            { 6,  1, 10 }, { 9, 0, 11 }, { 9, 11, 2 }, {  9, 2, 5 }, { 7, 2, 11 }
        };

        std::vector<Float3> spherePoints;
        for(int i = 0; i < 20; i++) {
                // note -- flip here to flip the winding
            GeodesicSphere_Subdivide(
                vdata[tindices[i][0]], vdata[tindices[i][2]], 
                vdata[tindices[i][1]], spherePoints, detail);
        }
        return spherePoints;
    }

    static std::vector<Internal::Vertex3D>   BuildGeodesicSphere()
    {
            //      build a geodesic sphere at the origin with radius 1     //
        auto pts = BuildGeodesicSpherePts();

        std::vector<Internal::Vertex3D> result;
        result.reserve(pts.size());

        for (auto i=pts.cbegin(); i!=pts.cend(); ++i) {
            Internal::Vertex3D vertex;
            vertex._position    = *i;
            vertex._normal      = Normalize(*i);        // centre is the origin, so normal points towards the position

                //  Texture coordinates based on longitude / latitude
            float latitude  = XlASin((*i)[2]);
            float longitude = XlATan2((*i)[1], (*i)[0]);
            vertex._texCoord = Float2(longitude, latitude);
            result.push_back(vertex);
        }
        return result;
    }

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
                std::make_pair(Internal::Vertex2D_InputLayout, dimof(Internal::Vertex2D_InputLayout)), shaderProgram);
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

        } else if (geoType == MaterialVisSettings::GeometryType::Sphere) {

            Metal::BoundInputLayout boundVertexInputLayout(
                std::make_pair(Internal::Vertex3D_InputLayout, dimof(Internal::Vertex3D_InputLayout)), shaderProgram);
            metalContext->Bind(boundVertexInputLayout);
            
            auto sphereGeometry = BuildGeodesicSphere();
            Metal::VertexBuffer vertexBuffer(AsPointer(sphereGeometry.begin()), sphereGeometry.size() * sizeof(Internal::Vertex3D));
            metalContext->Bind(MakeResourceList(vertexBuffer), sizeof(Internal::Vertex3D), 0);
            metalContext->Bind(Metal::Topology::TriangleList);
            metalContext->Draw(unsigned(sphereGeometry.size()));

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

    static bool WriteSystemVariable(
        const char name[], 
        const MaterialVisObject::SystemConstants& constants, 
        UInt2 viewportDims,
        void* destination, void* destinationEnd)
    {
        size_t size = size_t(destinationEnd) - size_t(destination);
        if (!_stricmp(name, "SI_OutputDimensions") && size >= (sizeof(unsigned)*2)) {
            ((unsigned*)destination)[0] = viewportDims[0];
            ((unsigned*)destination)[1] = viewportDims[1];
            return true;
        } else if (!_stricmp(name, "SI_NegativeLightDirection") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightNegativeDirection;
            return true;
        } else if (!_stricmp(name, "SI_LightColor") && size >= sizeof(Float3)) {
            *((Float3*)destination) = constants._lightColour;
            return true;
        }
        return false;
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

                        auto result = RenderCore::MakeSharedPkt(bufferDesc.Size);
                        std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                        bool foundAtLeastOneParameter = false;

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
                                auto param = constants.GetParameter(nameHash);
                                if (param.first) {

                                    auto type = reflectionVariable->GetType();
                                    D3D11_SHADER_TYPE_DESC typeDesc;
                                    hresult = type->GetDesc(&typeDesc);
                                    if (SUCCEEDED(hresult)) {

                                            //
                                            //      Finally, copy whatever the material object
                                            //      is, into the destination position in the 
                                            //      constant buffer;
                                            //  

                                        // ShaderPatcherLayer::TypeRules::CopyToBytes(
                                        //     PtrAdd(result.begin(), variableDesc.StartOffset), obj, 
                                        //     BuildTypeName(typeDesc), ShaderPatcherLayer::TypeRules::ExtractTypeName(obj),
                                        //     result.end());

                                        *(uint32*)PtrAdd(result.begin(), variableDesc.StartOffset) = param.second;
                                        foundAtLeastOneParameter = true;
                                    }

                                } else {
                                    
                                    foundAtLeastOneParameter |= WriteSystemVariable(
                                        variableDesc.Name, systemConstantsContext, viewportDims,
                                        PtrAdd(result.begin(), variableDesc.StartOffset), result.end());

                                }

                            }
                        }

                        if (foundAtLeastOneParameter) {
                            finalResult.push_back(
                                std::make_pair(Hash64(bindDesc.Name), std::move(result)));
                        }   
                    }
                }
            }

        }

        return finalResult;
    }

    static inline bool CompareRB(
        const RenderCore::Assets::MaterialParameters::ResourceBinding& lhs,
        const RenderCore::Assets::MaterialParameters::ResourceBinding& rhs)
    {
        return lhs._bindHash  < rhs._bindHash;
    }

    static std::vector<const RenderCore::Metal::ShaderResourceView*>
        BuildBoundTextures(
            RenderCore::Metal::BoundUniforms& boundUniforms,
            ID3D::ShaderReflection& reflection,
            const RenderCore::Assets::MaterialParameters::ResourceBindingSet& bindings)
    {
        using namespace RenderCore;
        typedef RenderCore::Assets::MaterialParameters::ResourceBinding ResourceBinding;
        std::vector<const Metal::ShaderResourceView*> result;

            //
            //      Find the texture binding points, and assign textures from
            //      the material parameters state to them.
            //

        D3D11_SHADER_DESC shaderDesc;
        reflection.GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection.GetResourceBindingDesc(c, &bindDesc);
            if  (bindDesc.Type == D3D10_SIT_TEXTURE) {

                std::string str;
                auto hash = Hash64(bindDesc.Name);
                auto i = std::lower_bound(bindings.begin(), bindings.end(), ResourceBinding(hash, std::string()), CompareRB);
                if (i != bindings.end() && i->_bindHash == hash) {
                    str = i->_resourceName;
                } else {
                        //  It's not mentioned in the material resources. try to look
                        //  for a default resource for this bind point
                    str = std::string("game/xleres/DefaultResources/") + bindDesc.Name + ".dds";
                }

                if (!str.empty()) {
                    TRY {
                        const Metal::DeferredShaderResource& texture = 
                            ::Assets::GetAssetDep<Metal::DeferredShaderResource>(str.c_str());

                        result.push_back(&texture.GetShaderResource());
                        boundUniforms.BindShaderResource(
                            Hash64(bindDesc.Name, &bindDesc.Name[XlStringLen(bindDesc.Name)]),
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

        return result;
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
            boundLayout.BindConstantBuffer(Hash64("LocalTransform"), 0, 1);
            for (auto i=materialConstants.cbegin(); i!=materialConstants.cend(); ++i) {
                boundLayout.BindConstantBuffer(i->first, unsigned(constantBufferPackets.size()), 1);
                constantBufferPackets.push_back(std::move(i->second));
            }

            auto boundTextures = BuildBoundTextures(
                boundLayout, 
                *_object->_shaderProgram->GetCompiledPixelShader().GetReflection(), 
                _object->_parameters._bindings);
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
                SceneEngine::LightingParser_ExecuteScene(context, parserContext, sceneParser, qualSettings);
            } else if (settings._lightingType == MaterialVisSettings::LightingType::Forward) {
                // no way to force the lighting parser to use a forward lighting pass currently
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

