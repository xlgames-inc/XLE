// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetalStubs.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace SceneEngine { namespace MetalStubs
{
	void GeometryShader::SetDefaultStreamOutputInitializers(const StreamOutputInitializers&)
	{
	}

	const GeometryShader::StreamOutputInitializers& GeometryShader::GetDefaultStreamOutputInitializers()
	{
		return *(const GeometryShader::StreamOutputInitializers*)nullptr;
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
}}
