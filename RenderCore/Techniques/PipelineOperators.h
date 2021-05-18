// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PipelineCollection.h"
#include "../../Assets/AssetsCore.h"

namespace RenderCore 
{
	class IThreadContext;
	class UniformsStream;
	class UniformsStreamInterface;
}

namespace RenderCore { namespace Techniques
{
	class IShaderOperator
	{
	public:
		virtual void Draw(IThreadContext&, const UniformsStream&) = 0;
		virtual ~IShaderOperator();
	};

	class RenderPassInstance;

	::Assets::FuturePtr<IShaderOperator> CreateFullViewportOperator(
		const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
		const RenderPassInstance& rpi,
		StringSection<> pixelShader,
		const UniformsStreamInterface& usi);

	::Assets::FuturePtr<IShaderOperator> CreateFullViewportOperator(
		const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
		const FrameBufferTarget& fbTarget,
		StringSection<> pixelShader,
		const UniformsStreamInterface& usi);
}}
