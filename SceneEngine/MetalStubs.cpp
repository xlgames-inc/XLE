// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalStubs.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/DX11/Metal/Buffer.h"
#include "../RenderCore/DX11/Metal/IncludeDX11.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11
	namespace RenderCore { namespace Metal_DX11
	{
		extern StreamOutputInitializers g_defaultStreamOutputInitializers;
	}}
#endif

namespace SceneEngine { namespace MetalStubs
{
	void SetDefaultStreamOutputInitializers(const RenderCore::StreamOutputInitializers& so)
	{
		#if GFXAPI_ACTIVE == GFXAPI_DX11
			RenderCore::Metal_DX11::g_defaultStreamOutputInitializers = so;
		#endif
	}

	RenderCore::StreamOutputInitializers GetDefaultStreamOutputInitializers()
	{
		#if GFXAPI_ACTIVE == GFXAPI_DX11
			return RenderCore::Metal_DX11::g_defaultStreamOutputInitializers;
		#else
			return GeometryShader::StreamOutputInitializers{};
		#endif
	}

	void UnbindSO(RenderCore::Metal::DeviceContext& devContext)
	{
        devContext.GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
	}

	void UnbindTessellationShaders(RenderCore::Metal::DeviceContext& devContext)
	{
		devContext.GetUnderlying()->HSSetShader(nullptr, nullptr, 0);
		devContext.GetUnderlying()->DSSetShader(nullptr, nullptr, 0);
	}

	void UnbindGeometryShader(RenderCore::Metal::DeviceContext& devContext)
	{
		devContext.GetUnderlying()->GSSetShader(nullptr, nullptr, 0);
	}

	RenderCore::Metal::NumericUniformsInterface& GetGlobalNumericUniforms(RenderCore::Metal::DeviceContext& devContext, RenderCore::ShaderStage stage)
	{
		return devContext.GetNumericUniforms(stage);
	}

#if GFXAPI_ACTIVE == GFXAPI_DX11
	void BindSO(RenderCore::Metal::DeviceContext& metalContext, RenderCore::IResource& res, unsigned offset)
	{
		auto* metalResource = (RenderCore::Metal::Resource*)res.QueryInterface(typeid(RenderCore::Metal::Resource).hash_code());
		ID3D::Buffer* underlying = (ID3D::Buffer*)metalResource->GetUnderlying().get();
        metalContext.GetUnderlying()->SOSetTargets(1, &underlying, &offset);
	}
#else
	void BindSO(RenderCore::Metal::DeviceContext&, unsigned slot, RenderCore::IResource& res, unsigned offset) {}
#endif
}}
