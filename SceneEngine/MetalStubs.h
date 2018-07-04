// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Types.h"
#include "../RenderCore/ResourceList.h"
#include "../RenderCore/Metal/DeviceContext.h"

namespace SceneEngine { namespace MetalStubs
{
	class GeometryShader
	{
	public:
		class StreamOutputInitializers
		{
		public:
			const RenderCore::InputElementDesc* _outputElements;
			unsigned _outputElementCount;
			const unsigned* _outputBufferStrides;
			unsigned _outputBufferCount;

			StreamOutputInitializers(
				const RenderCore::InputElementDesc outputElements[], unsigned outputElementCount,
				const unsigned outputBufferStrides[], unsigned outputBufferCount)
				: _outputElements(outputElements), _outputElementCount(outputElementCount)
				, _outputBufferStrides(outputBufferStrides), _outputBufferCount(outputBufferCount)
			{}
			StreamOutputInitializers()
				: _outputElements(nullptr), _outputElementCount(0)
				, _outputBufferStrides(nullptr), _outputBufferCount(0)
			{}
		};

		static void SetDefaultStreamOutputInitializers(const StreamOutputInitializers&);
		static const StreamOutputInitializers& GetDefaultStreamOutputInitializers();
	};


	template<typename Type, int Count>
		void BindSO(RenderCore::Metal::DeviceContext&, const RenderCore::ResourceList<Type, Count>&);
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

	template<typename Type, int Count>
		void BindSO(RenderCore::Metal::DeviceContext& devContext, const RenderCore::ResourceList<Type, Count>& res) {}

	template<typename Type> void UnbindVS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}
	template<typename Type> void UnbindPS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}
	template<typename Type> void UnbindGS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}
	template<typename Type> void UnbindCS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}
	template<typename Type> void UnbindHS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}
	template<typename Type> void UnbindDS(RenderCore::Metal::DeviceContext& devContext, unsigned slotStart, unsigned slotCount) {}

}}
