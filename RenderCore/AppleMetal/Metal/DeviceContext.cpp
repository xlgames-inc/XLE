// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "State.h"
#include "Shader.h"
#include "InputLayout.h"
#include "Buffer.h"
#include "Format.h"
#include "../IDeviceAppleMetal.h"
#include "../../IThreadContext.h"

#include <assert.h>

namespace RenderCore { namespace Metal_AppleMetal
{
    void GraphicsPipeline::Bind(const IndexBufferView& IB)
    {
        assert(0);
    }

    void GraphicsPipeline::Bind(Topology topology)
    {
        assert(0);
    }

    void GraphicsPipeline::Bind(const ShaderProgram& shaderProgram)
    {
        assert(0);
    }

    void GraphicsPipeline::Bind(const BlendState& blender)
    {
        assert(0);
    }

    void GraphicsPipeline::Bind(const RasterizationDesc& desc)
    {
        assert(0);
    }

    void GraphicsPipeline::Bind(const DepthStencilDesc& desc)
    {
        assert(0);
    }

    void GraphicsPipeline::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        assert(0);
    }

    void GraphicsPipeline::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        assert(0);
    }

    #if HACK_PLATFORM_IOS
        void GraphicsPipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation)
        {
            assert(0);
        }
    #endif


    DeviceContext::DeviceContext()
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
        auto* tc = (IThreadContextAppleMetal*)threadContext.QueryInterface(typeid(IThreadContextAppleMetal).hash_code());
        if (tc) return tc->GetDeviceContext();
        return dummy;
    }

    static ObjectFactory* s_objectFactory_instance = nullptr;

    ObjectFactory& GetObjectFactory(IDevice& device) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory(DeviceContext&) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory() { assert(s_objectFactory_instance); return *s_objectFactory_instance; }

}}

