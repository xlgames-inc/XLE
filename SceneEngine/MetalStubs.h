// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Types.h"
#include "../RenderCore/ResourceList.h"
#include "../RenderCore/Metal/DeviceContext.h"

namespace SceneEngine { namespace MetalStubs
{
	void SetDefaultStreamOutputInitializers(const RenderCore::StreamOutputInitializers&);
	RenderCore::StreamOutputInitializers GetDefaultStreamOutputInitializers();

	void BindSO(RenderCore::Metal::DeviceContext&, RenderCore::IResource& res, unsigned offset=0);
	void UnbindSO(RenderCore::Metal::DeviceContext&);
	void UnbindTessellationShaders(RenderCore::Metal::DeviceContext&);
	void UnbindGeometryShader(RenderCore::Metal::DeviceContext&);

	template<typename Type> void UnbindVS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);
	template<typename Type> void UnbindPS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);
	template<typename Type> void UnbindGS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);
	template<typename Type> void UnbindCS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);
	template<typename Type> void UnbindHS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);
	template<typename Type> void UnbindDS(RenderCore::Metal::DeviceContext&, unsigned slotStart, unsigned slotCount);

	RenderCore::Metal::NumericUniformsInterface& GetGlobalNumericUniforms(RenderCore::Metal::DeviceContext&, RenderCore::ShaderStage);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Type> void UnbindVS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) 
	{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Vertex).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}
	template<typename Type> void UnbindPS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) 
	{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Pixel).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}
	template<typename Type> void UnbindGS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount)
	{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Geometry).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}
	template<typename Type> void UnbindCS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount)
		{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Compute).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}
	template<typename Type> void UnbindHS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount)
	{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Hull).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}
	template<typename Type> void UnbindDS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount)
	{
		for (unsigned slot=0; slot<slotCount; ++slot) {
			GetGlobalNumericUniforms(devContext, RenderCore::ShaderStage::Domain).Bind(
				RenderCore::MakeResourceList(slotStart+slot, Type{}));
		}
	}

}}
