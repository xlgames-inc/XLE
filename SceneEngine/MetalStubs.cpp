// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalStubs.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/IDevice.h"

#if GFXAPI_TARGET == GFXAPI_DX11
	namespace RenderCore { namespace Metal_DX11
	{
		extern StreamOutputInitializers g_defaultStreamOutputInitializers;
	}}
	#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#elif GFXAPI_TARGET == GFXAPI_VULKAN
	#include "../RenderCore/Vulkan/Metal/IncludeVulkan.h"
	#include "../RenderCore/Vulkan/IDeviceVulkan.h"
#endif

namespace SceneEngine { namespace MetalStubs
{
	void SetDefaultStreamOutputInitializers(const RenderCore::StreamOutputInitializers& so)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			RenderCore::Metal_DX11::g_defaultStreamOutputInitializers = so;
		#endif
	}

	RenderCore::StreamOutputInitializers GetDefaultStreamOutputInitializers()
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			return RenderCore::Metal_DX11::g_defaultStreamOutputInitializers;
		#else
			return RenderCore::StreamOutputInitializers{};
		#endif
	}

	void UnbindTessellationShaders(RenderCore::Metal::DeviceContext& devContext)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			devContext.GetUnderlying()->HSSetShader(nullptr, nullptr, 0);
			devContext.GetUnderlying()->DSSetShader(nullptr, nullptr, 0);
		#endif
	}

	void UnbindGeometryShader(RenderCore::Metal::DeviceContext& devContext)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			devContext.GetUnderlying()->GSSetShader(nullptr, nullptr, 0);
		#endif
	}

	void UnbindComputeShader(RenderCore::Metal::DeviceContext& devContext)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			devContext.GetUnderlying()->CSSetShader(nullptr, nullptr, 0);
		#endif
	}

	void UnbindRenderTargets(RenderCore::Metal::DeviceContext& devContext)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			devContext.GetUnderlying()->OMSetRenderTargets(0, nullptr, nullptr);
		#endif
	}

	RenderCore::Metal::NumericUniformsInterface& GetGlobalNumericUniforms(RenderCore::Metal::DeviceContext& devContext, RenderCore::ShaderStage stage)
	{
		return devContext.GetNumericUniforms(stage);
	}

	void BindSO(RenderCore::Metal::DeviceContext& metalContext, RenderCore::IResource& res, unsigned offset)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			auto* metalResource = (RenderCore::Metal::Resource*)res.QueryInterface(typeid(RenderCore::Metal::Resource).hash_code());
			ID3D::Buffer* underlying = (ID3D::Buffer*)metalResource->GetUnderlying().get();
			metalContext.GetUnderlying()->SOSetTargets(1, &underlying, &offset);
		#elif GFXAPI_TARGET == GFXAPI_VULKAN

			auto deviceVulkan = (RenderCore::IDeviceVulkan*)RenderCore::Techniques::GetThreadContext()->GetDevice()->QueryInterface(typeid(RenderCore::IDeviceVulkan).hash_code());
			VkInstance instance = deviceVulkan->GetVulkanInstance();

			auto proc0 = (PFN_vkCmdBeginTransformFeedbackEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginTransformFeedbackEXT");
			auto proc1 = (PFN_vkCmdBindTransformFeedbackBuffersEXT)vkGetInstanceProcAddr(instance, "vkCmdBindTransformFeedbackBuffersEXT");

			(*proc0)(
				metalContext.GetActiveCommandList().GetUnderlying().get(),
				0, 0, nullptr, nullptr);

			VkDeviceSize offsets[] = { offset };
			VkDeviceSize sizes[] = { VK_WHOLE_SIZE };
			VkBuffer buffer[] = { ((RenderCore::Metal::Resource*)res.QueryInterface(typeid(RenderCore::Metal::Resource).hash_code()))->GetBuffer() };
			(*proc1)(
				metalContext.GetActiveCommandList().GetUnderlying().get(),
				0, 1, 
				buffer, offsets, sizes);
		#endif
	}

	void UnbindSO(RenderCore::Metal::DeviceContext& metalContext)
	{
		#if GFXAPI_TARGET == GFXAPI_DX11
			metalContext.GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
		#elif GFXAPI_TARGET == GFXAPI_VULKAN

			auto deviceVulkan = (RenderCore::IDeviceVulkan*)RenderCore::Techniques::GetThreadContext()->GetDevice()->QueryInterface(typeid(RenderCore::IDeviceVulkan).hash_code());
			VkInstance instance = deviceVulkan->GetVulkanInstance();

			auto proc0 = (PFN_vkCmdEndTransformFeedbackEXT)vkGetInstanceProcAddr(instance, "vkCmdEndTransformFeedbackEXT");

			(*proc0)(
				metalContext.GetActiveCommandList().GetUnderlying().get(),
				0, 0, nullptr, nullptr);
		#endif
	}
}}
