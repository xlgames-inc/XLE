
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/MinimalShaderSource.h"
#include "../RenderCore/ShaderService.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/AppleMetal/Device.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/RenderPassUtils.h"
#include "../RenderCore/ResourceUtils.h"
#include "../Assets/AssetServices.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include <memory>
#include <map>

namespace UnitTests
{
    class MetalTestHelper
    {
    public:
        RenderCore::CompiledShaderByteCode MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {});

        ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;
		std::shared_ptr<RenderCore::IDevice> _device;
		std::unique_ptr<RenderCore::ShaderService> _shaderService;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

        MetalTestHelper(RenderCore::UnderlyingAPI api);
        ~MetalTestHelper();
    };

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U N I T   T E S T   F B    H E L P E R

    class UnitTestFBHelper
    {
    public:
        RenderCore::Techniques::AttachmentPool _namedResources;
        RenderCore::Techniques::FrameBufferPool _frameBufferPool;
        RenderCore::Techniques::TechniqueContext _techniqueContext;
        RenderCore::Techniques::ParsingContext _parsingContext;     // careful init-order rules

        std::shared_ptr<RenderCore::IResource> _target;

        UnitTestFBHelper(RenderCore::IDevice& device, RenderCore::IThreadContext& threadContext, const RenderCore::ResourceDesc& targetDesc)
        : _parsingContext(_techniqueContext, &_namedResources, &_frameBufferPool)     // careful init-order rules
        {
            threadContext.CommitHeadless();
            _target = device.CreateResource(targetDesc);
            _threadContext = &threadContext;
        }

        ~UnitTestFBHelper()
        {
            if (_threadContext) {
                _threadContext->CommitHeadless();
            }
        }

        RenderCore::Techniques::RenderPassInstance BeginRenderPass(RenderCore::LoadStore loadStore = RenderCore::LoadStore::Clear)
        {
            RenderCore::Techniques::RenderPassInstance rpi = RenderCore::Techniques::RenderPassToPresentationTarget(*_threadContext, _target, _parsingContext, loadStore);
            return rpi;
        }

        std::map<unsigned, unsigned> GetFullColorBreakdown()
        {
            std::map<unsigned, unsigned> result;

            auto data = _target->ReadBack(*_threadContext);

            assert(data.size() == (size_t)RenderCore::ByteCount(_target->GetDesc()));
            auto pixels = MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()));
            for (auto p:pixels) ++result[p];

            return result;
        }

    private:
        RenderCore::IThreadContext* _threadContext;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U T I L I T Y    F N S

    static std::shared_ptr<RenderCore::IResource> CreateVB(RenderCore::IDevice& device, IteratorRange<const void*> data)
    {
        using namespace RenderCore;
        return device.CreateResource(
            CreateDesc(
                BindFlag::VertexBuffer, 0, GPUAccess::Read,
                LinearBufferDesc::Create((unsigned)data.size()),
                "vertexBuffer"),
            [data](SubResourceId) -> SubResourceInitData { return SubResourceInitData { data }; });
    }

    static RenderCore::Metal::ShaderProgram MakeShaderProgram(MetalTestHelper& testHelper, StringSection<> vs, StringSection<> ps)
    {
        return RenderCore::Metal::ShaderProgram(RenderCore::Metal::GetObjectFactory(), testHelper.MakeShader(vs, "vs_*"), testHelper.MakeShader(ps, "ps_*"));
    }

}

