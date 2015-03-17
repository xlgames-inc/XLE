// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderingUtils.h"
#include "LightingParserContext.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/RenderUtils.h"
#include "../Math/Transformations.h"

namespace SceneEngine
{

    void DrawBasisAxes(RenderCore::Metal::DeviceContext* context, const SceneEngine::LightingParserContext& parserContext, const Float3& offset)
    {
        TRY {

                //
                //      Draw world space X, Y, Z axes (to make it easier to see what's going on)
                //

            class Vertex
            {
            public:
                Float3      _position;
                unsigned    _color;
            };

            const float     size = 100.f;
            const Vertex    vertices[] = 
            {
                {   offset + Float3(0.f,     0.f,     0.f),  0xff0000ff },
                {   offset + Float3(size,    0.f,     0.f),  0xff0000ff },
                {   offset + Float3(0.f,     0.f,     0.f),  0xff00ff00 },
                {   offset + Float3(0.f,    size,     0.f),  0xff00ff00 },
                {   offset + Float3(0.f,     0.f,     0.f),  0xffff0000 },
                {   offset + Float3(0.f,     0.f,    size),  0xffff0000 }
            };

            using namespace RenderCore;
            using namespace RenderCore::Metal;
            InputElementDesc vertexInputLayout[] = {
                InputElementDesc( "POSITION", 0, NativeFormat::R32G32B32_FLOAT ),
                InputElementDesc( "COLOR", 0, NativeFormat::R8G8B8A8_UNORM )
            };

            VertexBuffer vertexBuffer(vertices, sizeof(vertices));

            using namespace RenderCore::Metal;
            using namespace RenderCore::Techniques;
            ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = MakeLocalTransformPacket(Identity<Float4x4>(), ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));

            ShaderProgram& shaderProgram = ::Assets::GetAsset<ShaderProgram>(
                "game/xleres/forward/illum.vsh:main:" VS_DefShaderModel, 
                "game/xleres/forward/illum.psh:main", 
                "GEO_HAS_COLOUR=1");
            BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
            context->Bind(boundVertexInputLayout);
            context->Bind(shaderProgram);
            context->Bind(     ResourceList<VertexBuffer, 1>(std::make_tuple(std::ref(vertexBuffer))), 
                                sizeof(Vertex), 0);

            BoundUniforms boundLayout(shaderProgram);
            static const auto HashLocalTransform = Hash64("LocalTransform");
            boundLayout.BindConstantBuffer(HashLocalTransform, 0, 1); // , RenderCore::Assets::LocalTransform_Elements, RenderCore::Assets::LocalTransform_ElementsCount);
            TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets)));

            context->Bind(Topology::LineList);
            context->Draw(dimof(vertices));

        } CATCH(const std::exception&) {
        } CATCH_END
    }


}