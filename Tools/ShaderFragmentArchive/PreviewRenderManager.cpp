// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"
#pragma warning(disable:4100)       // unreferenced formal parameter
#include "../../Core/Prefix.h"
#include "../../Core/WinAPI/IncludeWindows.h"
#include "PreviewRenderManager.h"
#include "TypeRules.h"
#include "ShaderDiagramDocument.h"
#include "../GUILayer/MarshalString.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/DeviceContextImpl.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
// #include "../../RenderCore/Assets/AssetUtils.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Math/Vector.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"
#include <tuple>
#include <D3D11Shader.h>

#include "../../RenderCore/DX11/Metal/IncludeDX11.h"

namespace RenderCore { namespace Metal_DX11 { std::unique_ptr<ShaderProgram> CreatePreviewShader(const std::string& nativeShaderText); } }
namespace RenderCore { namespace Assets { extern Float3 NegativeLightDirection; } }

namespace PreviewRender
{
    class ManagerPimpl
    {
    public:
        std::unique_ptr<RenderCore::IDevice>   _device;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
    };

    static intrusive_ptr<ID3D::Texture2D> CreateTexture(
        RenderCore::IDevice* device, 
        const D3D11_TEXTURE2D_DESC& desc)
    {
        RenderCore::Metal::ObjectFactory objectFactory(device);

        intrusive_ptr<ID3D::Texture2D> result;
        ID3D::Texture2D* tempTarget = nullptr;
        HRESULT hresult = objectFactory.GetUnderlying()->CreateTexture2D(&desc, nullptr, &tempTarget);
        if (tempTarget) {
            if (SUCCEEDED(hresult)) {
                result = moveptr(tempTarget);
            } else tempTarget->Release();
        }
        return result;
    }

    // static void WaitUntilReady(const RenderCore::Metal::CompiledShaderByteCode& shaderByteCode)
    // {
    //     for (;;)
    //     {
    //         TRY 
    //         {
    //             RenderCore::Metal_DX11::UpdateThreadPump();
    //             shaderByteCode.GetSize();
    //             break;
    //         } 
    //         CATCH (::Assets::Exceptions::PendingResource&) {}
    //         CATCH (...) {throw;}
    //         CATCH_END
    //     }
    // }

    static System::String^ BuildTypeName(const D3D11_SHADER_TYPE_DESC& typeDesc)
    {
        System::String^ typeName;

            //      Convert from the D3D type into a string
            //      type name
        if (typeDesc.Type == D3D10_SVT_FLOAT) {
            typeName = "float";
        } else if (typeDesc.Type == D3D10_SVT_INT) {
            typeName = "int";
        } else if (typeDesc.Type == D3D10_SVT_BOOL) {
            typeName = "bool";
        } else if (typeDesc.Type == D3D10_SVT_UINT) {
            typeName = "uint";
        } else {
            return "";
        }

        if (typeDesc.Columns > 1 || (typeDesc.Columns == 1 && typeDesc.Rows > 1)) {
            typeName += typeDesc.Columns;

            if (typeDesc.Rows > 1) {
                typeName += 'x';
                typeName += typeDesc.Rows;
            }
        }

        return typeName;
    }

    class MaterialConstantBuffer
    {
    public:
        std::string _bindingName;
        RenderCore::Metal::ConstantBufferPacket _buffer;
    };

    class SystemConstantsContext
    {
    public:
        Float3      _lightNegativeDirection;
        Float3      _lightColour;
        unsigned    _outputWidth, _outputHeight;
    };

    static bool WriteSystemVariable(const char name[], ShaderDiagram::Document^ doc, const SystemConstantsContext& context, void* destination, void* destinationEnd)
    {
        size_t size = size_t(destinationEnd) - size_t(destination);
        if (!_stricmp(name, "SI_OutputDimensions") && size >= (sizeof(unsigned)*2)) {
            ((unsigned*)destination)[0] = context._outputWidth;
            ((unsigned*)destination)[1] = context._outputHeight;
            return true;
        } else if (!_stricmp(name, "SI_NegativeLightDirection") && size >= sizeof(Float3)) {
            *((Float3*)destination) = doc->NegativeLightDirection;
            return true;
        } else if (!_stricmp(name, "SI_LightColor") && size >= sizeof(Float3)) {
            *((Float3*)destination) = context._lightColour;
            return true;
        }
        return false;
    }

    static std::vector<MaterialConstantBuffer> BuildMaterialConstants(ID3D::ShaderReflection* reflection, ShaderDiagram::Document^ doc, const SystemConstantsContext& systemConstantsContext)
    {

            //
            //      Find the cbuffers, and look for the variables
            //      within. Attempt to fill those values with the appropriate values
            //      from the current previewing material state
            //
        std::vector<MaterialConstantBuffer> finalResult;

        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(c, &bindDesc);

            if (bindDesc.Type == D3D10_SIT_CBUFFER) {
                auto cbuffer = reflection->GetConstantBufferByName(bindDesc.Name);
                if (cbuffer) {
                    D3D11_SHADER_BUFFER_DESC bufferDesc;
                    HRESULT hresult = cbuffer->GetDesc(&bufferDesc);
                    if (SUCCEEDED(hresult)) {

                        auto result = RenderCore::MakeSharedPkt(bufferDesc.Size);
                        std::fill((uint8*)result.begin(), (uint8*)result.end(), 0);
                        bool foundAtLeastOnParameter = false;

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

                                auto managedName = clix::marshalString<clix::E_UTF8>(variableDesc.Name);
                                if (doc->PreviewMaterialState->ContainsKey(managedName)) {
                                    auto obj = doc->PreviewMaterialState[managedName];

                                    auto type = reflectionVariable->GetType();
                                    D3D11_SHADER_TYPE_DESC typeDesc;
                                    hresult = type->GetDesc(&typeDesc);
                                    if (SUCCEEDED(hresult)) {

                                            //
                                            //      Finally, copy whatever the material object
                                            //      is, into the destination position in the 
                                            //      constant buffer;
                                            //  

                                        ShaderPatcherLayer::TypeRules::CopyToBytes(
                                            PtrAdd(result.begin(), variableDesc.StartOffset), obj, 
                                            BuildTypeName(typeDesc), ShaderPatcherLayer::TypeRules::ExtractTypeName(obj),
                                            result.end());
                                        foundAtLeastOnParameter = true;
                                    }
                                } else {
                                    
                                    foundAtLeastOnParameter |= WriteSystemVariable(
                                        variableDesc.Name, doc, systemConstantsContext,
                                        PtrAdd(result.begin(), variableDesc.StartOffset), result.end());

                                }

                            }
                        }

                        if (foundAtLeastOnParameter) {
                            MaterialConstantBuffer bind;
                            bind._bindingName = bindDesc.Name;
                            bind._buffer = std::move(result);
                            finalResult.push_back(bind);
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
            ID3D::ShaderReflection* reflection,
            ShaderDiagram::Document^ doc)
    {
        using namespace RenderCore;
        std::vector<const Metal::ShaderResourceView*> result;

            //
            //      Find the texture binding points, and assign textures from
            //      the material parameters state to them.
            //

        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {

            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(c, &bindDesc);
            if  (bindDesc.Type == D3D10_SIT_TEXTURE) {

                String^ str;
                auto managedName = clix::marshalString<clix::E_UTF8>(bindDesc.Name);
                if (doc->PreviewMaterialState->ContainsKey(managedName)) {
                    auto textureName = doc->PreviewMaterialState[managedName];
                    str = dynamic_cast<String^>(textureName);
                } else {
                        //  It's not mentioned in the material resources. try to look
                        //  for a default resource for this bind point
                    str = gcnew String("game/xleres/DefaultResources/") + gcnew String(bindDesc.Name) + gcnew String(".dds");
                }

                if (str) {
                    auto nativeString = clix::marshalString<clix::E_UTF8>(str);
                    const Metal::DeferredShaderResource& texture = 
                        ::Assets::GetAssetDep<Metal::DeferredShaderResource>(nativeString.c_str());

                    try {
                        result.push_back(&texture.GetShaderResource());
                        boundUniforms.BindShaderResource(
                            Hash64(bindDesc.Name, &bindDesc.Name[XlStringLen(bindDesc.Name)]),
                            unsigned(result.size()-1), 1);
                    }
                    catch (::Assets::Exceptions::InvalidResource& ) {}

                }
                
            } else if (bindDesc.Type == D3D10_SIT_SAMPLER) {

                    //  we should also bind samplers to something
                    //  reasonable, also...

            }
        }

        return result;
    }

    static bool PositionInputIs2D(ID3D::ShaderReflection* reflection)
    {
            //
            //      Try to find out if the "POSITION" entry is 2D (or 3d/4d)...
            //
        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);
        for (unsigned c=0; c<shaderDesc.InputParameters; ++c) {
            D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
            HRESULT hresult = reflection->GetInputParameterDesc(c, &paramDesc);
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

    static Float4x4 MakeCameraToWorld(const Float3& forward, const Float3& up, const Float3& position)
    {
        Float3 right            = Cross(forward, up);
        Float3 adjustedUp       = Normalize(Cross(right, forward));
        Float3 adjustedRight    = Normalize(Cross(forward, adjustedUp));

        return Float4x4(
            right[0], up[0], -forward[0], position[0],
            right[1], up[1], -forward[1], position[1],
            right[2], up[2], -forward[2], position[2],
            0.f, 0.f, 0.f, 1.f);
    }
    
    class PreviewBuilderPimpl
    {
    public:
        std::unique_ptr<RenderCore::Metal::CompiledShaderByteCode>      _vertexShader;
        std::unique_ptr<RenderCore::Metal::CompiledShaderByteCode>      _pixelShader;
        std::unique_ptr<RenderCore::Metal::ShaderProgram>               _shaderProgram;
        std::string                                                     _errorString;

        RenderCore::Metal::ShaderProgram & GetShaderProgram()
        {
            if (_shaderProgram)         { return *_shaderProgram.get(); }

                // can throw a "PendingResource" or "InvalidResource"
            try {
                auto program = std::make_unique<RenderCore::Metal::ShaderProgram>(std::ref(*_vertexShader), std::ref(*_pixelShader));
                _shaderProgram = std::move(program);
                return *program;
            } catch (const ::Assets::Exceptions::InvalidResource& exception) {
                _errorString = exception.what();
                throw exception;
            }
        }
    };

    RenderCore::Metal::ConstantBufferPacket SetupGlobalState(
        RenderCore::Metal::DeviceContext*   context,
        const RenderCore::Techniques::GlobalTransformConstants&  globalTransform)
    {
            // deprecated -- use scene engine to set default states
        // context->Bind(SceneEngine::CommonResources()._dssReadWrite);
        // context->Bind(SceneEngine::CommonResources()._blendStraightAlpha);
        // context->Bind(SceneEngine::CommonResources()._defaultRasterizer);
        // context->BindPS(MakeResourceList(SceneEngine::CommonResources()._defaultSampler));
        return RenderCore::MakeSharedPkt(globalTransform);
    }

    RenderCore::Metal::ConstantBufferPacket SetupGlobalState(
        RenderCore::Metal::DeviceContext*   context,
        const RenderCore::Techniques::CameraDesc&       camera)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        
        ViewportDesc viewportDesc(*context);
        Float4x4 worldToCamera = InvertOrthonormalTransform(camera._cameraToWorld);
        Float4x4 projectionMatrix = PerspectiveProjection(
            camera, viewportDesc.Width / float(viewportDesc.Height));

        Techniques::GlobalTransformConstants transformConstants;
        transformConstants._worldToClip = Combine(worldToCamera, projectionMatrix);
        transformConstants._viewToWorld = camera._cameraToWorld;

        transformConstants._worldSpaceView = ExtractTranslation(camera._cameraToWorld);
        transformConstants._minimalProjection = ExtractMinimalProjection(projectionMatrix);
        transformConstants._farClip = Techniques::CalculateNearAndFarPlane(transformConstants._minimalProjection, Techniques::GetDefaultClipSpaceType()).second;

        // auto right      = Normalize(ExtractRight_Cam(camera._cameraToWorld));
        // auto up         = Normalize(ExtractUp_Cam(camera._cameraToWorld));
        // auto forward    = Normalize(ExtractForward_Cam(camera._cameraToWorld));
        // auto fy         = XlTan(0.5f * camera._verticalFieldOfView);
        // auto fx         = fy * viewportDesc.Width / float(viewportDesc.Height);
        // transformConstants._frustumCorners[0] = Expand(Float3(camera._farClip * (forward - fx * right + fy * up)), 1.f);
        // transformConstants._frustumCorners[1] = Expand(Float3(camera._farClip * (forward - fx * right - fy * up)), 1.f);
        // transformConstants._frustumCorners[2] = Expand(Float3(camera._farClip * (forward + fx * right + fy * up)), 1.f);
        // transformConstants._frustumCorners[3] = Expand(Float3(camera._farClip * (forward + fx * right - fy * up)), 1.f);

        const float aspectRatio = viewportDesc.Width / float(viewportDesc.Height);
        const float top = camera._nearClip * XlTan(.5f * camera._verticalFieldOfView);
        const float right = top * aspectRatio;
        Float3 preTransformCorners[] = {
            Float3(-right,  top, -camera._nearClip),
            Float3(-right, -top, -camera._nearClip),
            Float3( right,  top, -camera._nearClip),
            Float3( right, -top, -camera._nearClip) 
        };
        for (unsigned c=0; c<4; ++c) {
            transformConstants._frustumCorners[c] = 
                Expand(TransformDirectionVector(camera._cameraToWorld, preTransformCorners[c]), 1.f);
        }
        
        return SetupGlobalState(context, transformConstants);
    }

    enum DrawPreviewResult
    {
        DrawPreviewResult_Error,
        DrawPreviewResult_Pending,
        DrawPreviewResult_Success
    };

    static DrawPreviewResult DrawPreview(RenderCore::Metal::DeviceContext* context, PreviewBuilderPimpl& builder, ShaderDiagram::Document^ doc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;

            //
            //      \todo -- also support geometry shaders (for example, for building graphs from shader code)
            //
        try {

            auto& shaderProgram = builder.GetShaderProgram();
            context->Bind(shaderProgram);

            RasterizerState rastState(CullMode::Back);
            context->Bind(rastState);

                //
                //      Constants / Resources
                //

            RenderCore::Techniques::CameraDesc camera;
            camera._cameraToWorld = MakeCameraToWorld(Float3(1,0,0), Float3(0,0,1), Float3(-5,0,0));

            RenderCore::Metal::ViewportDesc currentViewport(*context);
            SystemConstantsContext systemConstantsContext;
            systemConstantsContext._outputWidth = (unsigned)currentViewport.Width;
            systemConstantsContext._outputHeight = (unsigned)currentViewport.Height;
            systemConstantsContext._lightNegativeDirection = Normalize(Float3(-1.f, 0.f, 1.f));
            systemConstantsContext._lightColour = Float3(1,1,1);
            auto materialConstants = BuildMaterialConstants(builder._pixelShader->GetReflection().get(), doc, systemConstantsContext);
        
            std::vector<RenderCore::Metal::ConstantBufferPacket> constantBufferPackets;
            constantBufferPackets.push_back(SetupGlobalState(context, camera));
            constantBufferPackets.push_back(Techniques::MakeLocalTransformPacket(Identity<Float4x4>(), camera));

            BoundUniforms boundLayout(shaderProgram);
            boundLayout.BindConstantBuffer(   Hash64("GlobalTransform"), 0, 1); //, Assets::GlobalTransform_Elements, Assets::GlobalTransform_ElementsCount);
            boundLayout.BindConstantBuffer(    Hash64("LocalTransform"), 1, 1); //, Assets::LocalTransform_Elements, Assets::LocalTransform_ElementsCount);
            for (auto i=materialConstants.cbegin(); i!=materialConstants.cend(); ++i) {
                boundLayout.BindConstantBuffer(Hash64(i->_bindingName), unsigned(constantBufferPackets.size()), 1);
                constantBufferPackets.push_back(i->_buffer);
            }

            auto boundTextures = BuildBoundTextures(boundLayout, builder._pixelShader->GetReflection().get(), doc);
            boundLayout.Apply(
                *context, 
                UniformsStream(),
                UniformsStream( AsPointer(constantBufferPackets.begin()), nullptr, constantBufferPackets.size(), 
                                AsPointer(boundTextures.begin()), boundTextures.size()));

                // disable blending to avoid problem when rendering single component stuff (ie, nodes that output "float", not "float4")
            context->Bind(BlendOp::NoBlending);

                //
                //      Geometry
                //
                //          There are different types of preview geometry we can feed into the system
                //          Plane2D         -- this is just a flat 2D plane covering the whole surface
                //          GeodesicSphere  -- this is a basic 3D sphere, with normals and texture coordinates
                //

            enum PreviewType
            {
                Plane2D,
                GeodesicSphere
            } previewType = GeodesicSphere;

            if (PositionInputIs2D(builder._vertexShader->GetReflection().get())) {
                previewType = Plane2D;
            }

            if (previewType == Plane2D) {

                BoundInputLayout boundVertexInputLayout(
                    std::make_pair(Internal::Vertex2D_InputLayout, dimof(Internal::Vertex2D_InputLayout)), shaderProgram);
                context->Bind(boundVertexInputLayout);
        
                const Internal::Vertex2D    vertices[] = 
                {
                    { Float2(-1.f, -1.f),  Float2(0.f, 0.f) },
                    { Float2( 1.f, -1.f),  Float2(1.f, 0.f) },
                    { Float2(-1.f,  1.f),  Float2(0.f, 1.f) },
                    { Float2( 1.f,  1.f),  Float2(1.f, 1.f) }
                };

                VertexBuffer vertexBuffer(vertices, sizeof(vertices));
                context->Bind(ResourceList<VertexBuffer, 1>(std::make_tuple(std::ref(vertexBuffer))), sizeof(Internal::Vertex2D), 0);
                context->Bind(Topology::TriangleStrip);
                context->Draw(dimof(vertices));

            } else if (previewType == GeodesicSphere) {

                BoundInputLayout boundVertexInputLayout(
                    std::make_pair(Internal::Vertex3D_InputLayout, dimof(Internal::Vertex3D_InputLayout)), shaderProgram);
                context->Bind(boundVertexInputLayout);
            
                auto sphereGeometry = BuildGeodesicSphere();
                VertexBuffer vertexBuffer(AsPointer(sphereGeometry.begin()), sphereGeometry.size() * sizeof(Internal::Vertex3D));
                context->Bind(ResourceList<VertexBuffer, 1>(std::make_tuple(std::ref(vertexBuffer))), sizeof(Internal::Vertex3D), 0);
                context->Bind(Topology::TriangleList);
                context->Draw(unsigned(sphereGeometry.size()));

            }

            return DrawPreviewResult_Success;
        }
        catch (::Assets::Exceptions::PendingResource&) { return DrawPreviewResult_Pending; }
        catch (::Assets::Exceptions::InvalidResource&) {}

        return DrawPreviewResult_Error;
    }

    System::Drawing::Bitmap^    PreviewBuilder::GenerateErrorBitmap(const char str[], Size^ size)
    {
            //      Previously, we got an error while rendering this item.
            //      Render some text to the bitmap with an error string. Just
            //      use the gdi for this (don't bother rendering via D3D)

        using System::Drawing::Bitmap;
        using namespace System::Drawing;
        Bitmap^ newBitmap = gcnew Bitmap(size->Width, size->Height, Imaging::PixelFormat::Format32bppArgb);

        Graphics^ dc = Graphics::FromImage(newBitmap);
        dc->FillRectangle(gcnew SolidBrush(Color::Black), 0, 0, newBitmap->Width, newBitmap->Height);
        dc->DrawString(gcnew String(str), gcnew Font("Arial", 9), gcnew SolidBrush(Color::White), RectangleF(0.f, 0.f, float(newBitmap->Width), float(newBitmap->Height)));
        delete dc;

        return newBitmap;
    }

    System::Drawing::Bitmap^    PreviewBuilder::GenerateBitmap(ShaderDiagram::Document^ doc, Size^ size)
    {
        const int width = std::max(0, int(size->Width));
        const int height = std::max(0, int(size->Height));

        if (!_pimpl->_errorString.empty()) {
            return GenerateErrorBitmap(_pimpl->_errorString.c_str(), size);
        }

            // note --  call GetShaderProgram() to check to see if our compile
            //          is ready. Do this before creating textures, etc -- to minimize
            //          overhead in the main thread while shaders are still compiling
        try {
            _pimpl->GetShaderProgram();
        } catch (const ::Assets::Exceptions::PendingResource&) {
            return nullptr; // still pending
        }
        catch (...) {}

        using namespace RenderCore;
        using namespace RenderCore::Metal;
        auto context = DeviceContext::Get(*Manager::Instance->GetDevice()->GetImmediateContext());

        D3D11_TEXTURE2D_DESC targetDesc;
        targetDesc.Width                = width;
        targetDesc.Height               = height;
        targetDesc.MipLevels            = 1;
        targetDesc.ArraySize            = 1;
        targetDesc.Format               = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        targetDesc.SampleDesc.Count     = 1;
        targetDesc.SampleDesc.Quality   = 0;
        targetDesc.Usage                = D3D11_USAGE_DEFAULT;
        targetDesc.BindFlags            = D3D11_BIND_RENDER_TARGET;
        targetDesc.CPUAccessFlags       = 0;
        targetDesc.MiscFlags            = 0;

        auto targetTexture = CreateTexture(Manager::Instance->GetDevice(), targetDesc);
        intrusive_ptr<ID3D::RenderTargetView> renderTargetView;
        if (targetTexture) {
            D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
            viewDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;       // \todo -- SRGB correction
            viewDesc.ViewDimension          = D3D11_RTV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice     = 0;
            {
                ID3D::RenderTargetView* tempView = nullptr;
                RenderCore::Metal::ObjectFactory objectFactory(Manager::Instance->GetDevice());
                HRESULT hresult = objectFactory.GetUnderlying()->CreateRenderTargetView(targetTexture.get(), &viewDesc, &tempView);
                if (tempView) {
                    if (SUCCEEDED(hresult)) {
                        renderTargetView = moveptr(tempView);
                    } else tempView->Release();
                }
            }
        }

        {
            ID3D::RenderTargetView* rtView = renderTargetView.get();
            context->GetUnderlying()->OMSetRenderTargets(1, &rtView, nullptr);

            D3D11_VIEWPORT viewport;
            viewport.TopLeftX = viewport.TopLeftY = 0;
            viewport.Width = FLOAT(width); 
            viewport.Height = FLOAT(height);
            viewport.MinDepth = 0.f; viewport.MaxDepth = 1.f;
            context->GetUnderlying()->RSSetViewports(1, &viewport);

            FLOAT clearColor[] = {0.05f, 0.05f, 0.2f, 1.f};
            context->GetUnderlying()->ClearRenderTargetView(rtView, clearColor);
        }

            ////////////

        auto result = DrawPreview(context.get(), *_pimpl, doc);
        if (result == DrawPreviewResult_Error) {
            return GenerateErrorBitmap(_pimpl->_errorString.c_str(), size);
        } else if (result == DrawPreviewResult_Pending) {
            return nullptr;
        }

            ////////////

        D3D11_TEXTURE2D_DESC readableTargetDesc = targetDesc;
        readableTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        readableTargetDesc.Usage = D3D11_USAGE_STAGING;
        readableTargetDesc.BindFlags = 0;
        readableTargetDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        intrusive_ptr<ID3D::Texture2D> readableTarget = CreateTexture(Manager::Instance->GetDevice(), readableTargetDesc);

        context->GetUnderlying()->CopyResource(readableTarget.get(), targetTexture.get());

        D3D11_MAPPED_SUBRESOURCE mappedTexture;
        HRESULT hresult = context->GetUnderlying()->Map(readableTarget.get(), 0, D3D11_MAP_READ, 0, &mappedTexture);
        if (SUCCEEDED(hresult)) {
            using System::Drawing::Bitmap;
            using namespace System::Drawing;
            Bitmap^ newBitmap = gcnew Bitmap(width, height, Imaging::PixelFormat::Format32bppArgb);
            auto data = newBitmap->LockBits(System::Drawing::Rectangle(0, 0, width, height), Imaging::ImageLockMode::WriteOnly, Imaging::PixelFormat::Format32bppArgb);
            try
            {
                    // we have to flip ABGR -> ARGB!
                for (int y=0; y<height; ++y) {
                    void* sourcePtr = PtrAdd(mappedTexture.pData, y * mappedTexture.RowPitch);
                    System::IntPtr destinationPtr = data->Scan0 + y * width * sizeof(unsigned);
                    for (int x=0; x<width; ++x) {
                        ((unsigned*)(void*)destinationPtr)[x] = 
                            (RenderCore::ARGBtoABGR(((unsigned*)sourcePtr)[x]) & 0x00ffffff) | 0xff000000;
                    }
                }
                // XlCopyMemory((void*)ptr, mappedTexture.pData, mappedTexture.DepthPitch);
            }
            finally
            {
                newBitmap->UnlockBits(data);
            }

            context->GetUnderlying()->Unmap(readableTarget.get(), 0);
            return newBitmap;
        }

        return nullptr;
    }

    void    PreviewBuilder::Update(ShaderDiagram::Document^ doc, Size^ size)
    {
        _bitmap = GenerateBitmap(doc, size);
    }

    void    PreviewBuilder::Invalidate()
    {
        delete _bitmap;
        _bitmap = nullptr;
    }

    PreviewBuilder::PreviewBuilder(System::String^ shaderText)
    {
        _pimpl = new PreviewBuilderPimpl();

        std::string nativeShaderText = clix::marshalString<clix::E_UTF8>(shaderText);
        try {
            using namespace RenderCore::Metal;
            _pimpl->_vertexShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "VertexShaderEntry", VS_DefShaderModel, "SHADER_NODE_EDITOR=1");
            _pimpl->_pixelShader = std::make_unique<CompiledShaderByteCode>(nativeShaderText.c_str(), "PixelShaderEntry", PS_DefShaderModel, "SHADER_NODE_EDITOR=1");
        }
        catch (::Assets::Exceptions::PendingResource&) {}
        catch (::Assets::Exceptions::InvalidResource&) 
        {
            _pimpl->_errorString = "Compile failure";
        }
    }

    PreviewBuilder::~PreviewBuilder()
    {
        delete _pimpl;
    }

    PreviewBuilder^    Manager::CreatePreview(System::String^ shaderText)
    {
        return gcnew PreviewBuilder(shaderText);
    }

    void                    Manager::RotateLightDirection(ShaderDiagram::Document^ doc, System::Drawing::PointF rotationAmount)
    {
        try {
            float deltaCameraYaw    = -rotationAmount.Y * 1.f * gPI / 180.f;
            float deltaCameraPitch  =  rotationAmount.X * 1.f * gPI / 180.f;

            Float3x3 rotationPart;
            cml::matrix_rotation_euler(rotationPart, deltaCameraYaw, 0.f, deltaCameraPitch, cml::euler_order_yxz);

            auto negLightDir = doc->NegativeLightDirection;
            negLightDir = TransformDirectionVector(rotationPart, negLightDir);
            doc->NegativeLightDirection = Normalize(negLightDir);
        } catch(...) {
            doc->NegativeLightDirection = Float3(0.f, 0.f, 1.f);        // catch any math errors
        }
    }

    RenderCore::IDevice*        Manager::GetDevice()
    {
        return _pimpl->_device.get();
    }

    static Manager::Manager()
    {
        _instance = nullptr;
    }

    Manager::Manager()
    {
        _pimpl = new ManagerPimpl;
        _pimpl->_device = RenderCore::CreateDevice();
        _pimpl->_asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
    }

    Manager::~Manager()
    {
        delete _pimpl;
    }


}

