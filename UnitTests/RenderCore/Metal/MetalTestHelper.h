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

		std::shared_ptr<RenderCore::IDevice> _device;
		std::unique_ptr<RenderCore::ShaderService> _shaderService;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

        MetalTestHelper(RenderCore::UnderlyingAPI api);
		MetalTestHelper(const std::shared_ptr<RenderCore::IDevice>& device);
        ~MetalTestHelper();
    };

    std::unique_ptr<MetalTestHelper> MakeTestHelper();

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

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U T I L I T Y    F N S

    std::shared_ptr<RenderCore::IResource> CreateVB(RenderCore::IDevice& device, IteratorRange<const void*> data);
    RenderCore::Metal::ShaderProgram MakeShaderProgram(MetalTestHelper& testHelper, StringSection<> vs, StringSection<> ps);

}

