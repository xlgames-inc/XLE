// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../RenderCore/Init.h"
#include "../../../RenderCore/ShaderService.h"
#include "../../../RenderCore/FrameBufferDesc.h"
#include "../../../RenderCore/Metal/Shader.h"
#include <memory>
#include <map>

namespace RenderCore { class IDevice; class IThreadContext; class DescriptorSetSignature; class FrameBufferDesc; }
namespace UnitTests
{
    class MetalTestHelper
    {
    public:
        RenderCore::CompiledShaderByteCode MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {});
        RenderCore::Metal::ShaderProgram MakeShaderProgram(StringSection<> vs, StringSection<> ps);
        
        std::shared_ptr<RenderCore::IResource> CreateVB(IteratorRange<const void*> data);
        std::shared_ptr<RenderCore::IResource> CreateIB(IteratorRange<const void*> data);
        std::shared_ptr<RenderCore::IResource> CreateCB(IteratorRange<const void*> data);

		std::shared_ptr<RenderCore::IDevice> _device;
		std::unique_ptr<RenderCore::ShaderService> _shaderService;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

        std::shared_ptr<RenderCore::ICompiledPipelineLayout> _pipelineLayout;
        std::unique_ptr<RenderCore::LegacyRegisterBindingDesc> _defaultLegacyBindings;

        void BeginFrameCapture();
        void EndFrameCapture();

        MetalTestHelper(RenderCore::UnderlyingAPI api);
		MetalTestHelper(const std::shared_ptr<RenderCore::IDevice>& device);
        ~MetalTestHelper();
    };

    std::unique_ptr<MetalTestHelper> MakeTestHelper();

    RenderCore::CompiledShaderByteCode MakeShader(
        const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& shaderSource, 
        StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {});
    RenderCore::Metal::ShaderProgram MakeShaderProgram(
        const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& shaderSource,
        const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
        StringSection<> vs, StringSection<> ps);

    std::shared_ptr<RenderCore::ILowLevelCompiler> CreateDefaultShaderCompiler(RenderCore::IDevice& device);

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U N I T   T E S T   F B    H E L P E R

    class UnitTestFBHelper
    {
    public:
        UnitTestFBHelper(
            RenderCore::IDevice& device,
            RenderCore::IThreadContext& threadContext,
            const RenderCore::ResourceDesc& mainTargetDesc,
            RenderCore::LoadStore beginLoadStore = RenderCore::LoadStore::Clear);
        ~UnitTestFBHelper();

        class IRenderPassToken
        {
        public:
            virtual ~IRenderPassToken() = default;
        };

        std::shared_ptr<IRenderPassToken> BeginRenderPass(RenderCore::IThreadContext& threadContext);
        std::map<unsigned, unsigned> GetFullColorBreakdown(RenderCore::IThreadContext& threadContext);
        const std::shared_ptr<RenderCore::IResource> GetMainTarget() const;
        const RenderCore::FrameBufferDesc& GetDesc() const;
        void SaveImage(RenderCore::IThreadContext& threadContext, StringSection<> filename) const;

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class DescriptorSetHelper
	{
	public:
		void Bind(unsigned descriptorSetSlot, const std::shared_ptr<RenderCore::IResourceView>&);
		void Bind(unsigned descriptorSetSlot, const std::shared_ptr<RenderCore::ISampler>&);
		std::shared_ptr<RenderCore::IDescriptorSet> CreateDescriptorSet(
            RenderCore::IDevice& device, 
            const RenderCore::DescriptorSetSignature& signature);

        DescriptorSetHelper();
        ~DescriptorSetHelper();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
	};

}

