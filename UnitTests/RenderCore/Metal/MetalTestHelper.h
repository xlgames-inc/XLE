// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../RenderCore/Init.h"
#include "../../../RenderCore/ShaderService.h"
#include "../../../RenderCore/Metal/Shader.h"
#include <memory>
#include <map>

namespace RenderCore { class IDevice; class IThreadContext; }
namespace UnitTests
{
    class MetalTestHelper
    {
    public:
        RenderCore::CompiledShaderByteCode MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {});
        RenderCore::Metal::ShaderProgram MakeShaderProgram(StringSection<> vs, StringSection<> ps);
        
        std::shared_ptr<RenderCore::IResource> CreateVB(IteratorRange<const void*> data);
        std::shared_ptr<RenderCore::IResource> CreateIB(IteratorRange<const void*> data);

		std::shared_ptr<RenderCore::IDevice> _device;
		std::unique_ptr<RenderCore::ShaderService> _shaderService;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

        std::shared_ptr<RenderCore::ICompiledPipelineLayout> _pipelineLayout;

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

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U N I T   T E S T   F B    H E L P E R

    class UnitTestFBHelper
    {
    public:
        UnitTestFBHelper(
            RenderCore::IDevice& device, 
            const RenderCore::ResourceDesc& mainTargetDesc);
        ~UnitTestFBHelper();

        class IRenderPassToken
        {
        public:
            virtual ~IRenderPassToken() = default;
        };

        std::shared_ptr<IRenderPassToken> BeginRenderPass(RenderCore::IThreadContext& threadContext);
        std::map<unsigned, unsigned> GetFullColorBreakdown(RenderCore::IThreadContext& threadContext);
        const std::shared_ptr<RenderCore::IResource> GetMainTarget() const;

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

}

