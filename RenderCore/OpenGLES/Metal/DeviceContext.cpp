// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "State.h"
#include "Shader.h"
#include "InputLayout.h"
#include "Buffer.h"
#include "Format.h"
#include "../../IThreadContext.h"

#include "IncludeGLES.h"
#include <assert.h>

namespace RenderCore { namespace Metal_OpenGLES
{
    static GLenum AsGLIndexBufferType(Format idxFormat)
    {
        GLenum glFormat = GL_UNSIGNED_SHORT;
        switch (idxFormat) {
        case Format::R8_UINT: glFormat = GL_UNSIGNED_BYTE; break;
        case Format::R16_UINT: glFormat = GL_UNSIGNED_SHORT; break;
        case Format::R32_UINT: glFormat = GL_UNSIGNED_INT; break;
        default: assert(0);
        }
        return glFormat;
    }

    void GraphicsPipeline::Bind(const IndexBufferView& IB)
    {
        assert(IB._offset == 0);    // (not supported currently... But we could safe it up for the draw call)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetBufferRawGLHandle(*IB._resource));

        // Note that Format::R32_UINT is only supported on OGLES3.0+
        assert(IB._indexFormat == Format::R32_UINT || IB._indexFormat == Format::R16_UINT || IB._indexFormat == Format::R8_UINT);
        _indicesFormat = AsGLIndexBufferType(IB._indexFormat);
        _indexFormatBytes = BitsPerPixel(IB._indexFormat) / 8;
    }

    void GraphicsPipeline::Bind(Topology topology)
    {
        _nativeTopology = AsGLenum(topology);
    }

    void GraphicsPipeline::Bind(const ShaderProgram& shaderProgram)
    {
        glUseProgram(shaderProgram.GetUnderlying()->AsRawGLHandle());
    }

    void GraphicsPipeline::Bind(const BlendState& blender)
    {
        blender.Apply();
    }

    static void SetUnmanagedStates()
    {
        // The following render states are not managed by RenderCore, but we can use this function
        // to set them all to rational defaults. In many cases, this may be just the same as the
        // default; but it's still useful incase some other system is setting them, and we want to
        // reset to something that won't interfere with our rendering.

        // RasterizationDesc unmanaged states
        glLineWidth(1.f);
        glPolygonOffset(0.f, 0.f);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DITHER);       // (not supported in D3D11)
    }

    void GraphicsPipeline::Bind(const RasterizationDesc& desc)
    {
        if (desc._cullMode != CullMode::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(AsGLenum(desc._cullMode));
        } else {
            glDisable(GL_CULL_FACE);
        }
        glFrontFace(AsGLenum(desc._frontFaceWinding));
    }

    void GraphicsPipeline::Bind(const DepthStencilDesc& desc)
    {
        glDepthFunc(AsGLenum(desc._depthTest));
        glDepthMask(desc._depthWrite ? GL_TRUE : GL_FALSE);

        if (desc._depthTest != CompareOp::Always) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (desc._stencilEnable) {
            glStencilFuncSeparate(
                GL_FRONT,
                AsGLenum(desc._frontFaceStencil._comparisonOp),
                desc._stencilReference,
                desc._stencilWriteMask);
            glStencilOpSeparate(
                GL_FRONT,
                AsGLenum(desc._frontFaceStencil._failOp),
                AsGLenum(desc._frontFaceStencil._depthFailOp),
                AsGLenum(desc._frontFaceStencil._passOp));
            glStencilFuncSeparate(
                GL_BACK,
                AsGLenum(desc._backFaceStencil._comparisonOp),
                desc._stencilReference,
                desc._stencilWriteMask);
            glStencilOpSeparate(
                GL_BACK,
                AsGLenum(desc._backFaceStencil._failOp),
                AsGLenum(desc._backFaceStencil._depthFailOp),
                AsGLenum(desc._backFaceStencil._passOp));
            glStencilMaskSeparate(GL_FRONT_AND_BACK, desc._stencilReadMask);
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }

    void GraphicsPipeline::Bind(const ViewportDesc& viewport)
    {
        glViewport((GLint)viewport.TopLeftX, (GLint)viewport.TopLeftY, (GLsizei)viewport.Width, (GLsizei)viewport.Height);

        // hack -- desktop gl has a slight naming change
        #if defined(GL_ES_VERSION_3_0) || defined(GL_ES_VERSION_2_0)
            glDepthRangef(viewport.MinDepth, viewport.MaxDepth);
        #else
            glDepthRange(viewport.MinDepth, viewport.MaxDepth);
        #endif
    }

    void GraphicsPipeline::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        glDrawArrays(GLenum(_nativeTopology), startVertexLocation, vertexCount);
    }

    void GraphicsPipeline::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);  // (doesn't seem to be supported. Maybe best to remove it from the interface)
        glDrawElements(
            GLenum(_nativeTopology), GLsizei(indexCount),
            GLenum(_indicesFormat),
            (const void*)(size_t)(_indexFormatBytes * startIndexLocation));
    }

    void GraphicsPipeline::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
    {
        #if defined(GL_ES_VERSION_2_0) || defined(GL_ES_VERSION_3_0)
            assert(_featureSet & FeatureSet::GLES300);
            glDrawArraysInstanced(
                GLenum(_nativeTopology),
                startVertexLocation, vertexCount,
                instanceCount);
        #else
            glDrawArraysInstancedARB(
                GLenum(_nativeTopology),
                startVertexLocation, vertexCount,
                instanceCount);
        #endif
    }

    void GraphicsPipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(baseVertexLocation==0);  // (doesn't seem to be supported. Maybe best to remove it from the interface)
        #if defined(GL_ES_VERSION_2_0) || defined(GL_ES_VERSION_3_0)
            assert(_featureSet & FeatureSet::GLES300);
            glDrawElementsInstanced(
                GLenum(_nativeTopology), GLsizei(indexCount),
                GLenum(_indicesFormat),
                (const void*)(size_t)(_indexFormatBytes * startIndexLocation),
                instanceCount);
        #else
            glDrawElementsInstancedARB(
                GLenum(_nativeTopology), GLsizei(indexCount),
                GLenum(_indicesFormat),
                (const void*)(size_t)(_indexFormatBytes * startIndexLocation),
                instanceCount);
        #endif
    }

    GraphicsPipeline::GraphicsPipeline(FeatureSet::BitField featureSet)
    {
        _indicesFormat = AsGLIndexBufferType(Format::R16_UINT);
        _indexFormatBytes = 2;
        _nativeTopology = GL_TRIANGLES;
        _featureSet = featureSet;
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
    }

    DeviceContext::DeviceContext(FeatureSet::BitField featureSet)
    : GraphicsPipeline(featureSet)
    {
    }

    DeviceContext::~DeviceContext()
    {
    }

    void                            DeviceContext::BeginCommandList()
    {   
    }

    intrusive_ptr<CommandList>         DeviceContext::ResolveCommandList()
    {
        return intrusive_ptr<CommandList>();
    }

    void                            DeviceContext::CommitCommandList(CommandList& commandList)
    {

    }

    const std::shared_ptr<DeviceContext>& DeviceContext::Get(IThreadContext& threadContext)
    {
        static std::shared_ptr<DeviceContext> dummy;
        auto* tc = (IThreadContextOpenGLES*)threadContext.QueryInterface(typeid(IThreadContextOpenGLES).hash_code());
        if (tc) return tc->GetDeviceContext();
        return dummy;
    }

}}

