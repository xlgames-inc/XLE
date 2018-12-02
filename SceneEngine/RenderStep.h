// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "SceneParser.h"
#include "../RenderCore/Techniques/Drawables.h"
#include <memory>

namespace RenderCore { namespace Techniques 
{
	class FrameBufferDescFragment;
	class RenderPassFragment;
}}

namespace SceneEngine
{
	class IViewDelegate
	{
	public:
		virtual RenderCore::Techniques::DrawablesPacket& GetDrawablesPacket(BatchFilter batch) = 0;
		virtual ~IViewDelegate();
	};

	class LightingParserContext;

	class IRenderStep
	{
	public:
		virtual std::shared_ptr<IViewDelegate> CreateViewDelegate();
		virtual const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const = 0;
		virtual void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate) = 0;
		virtual ~IRenderStep();
	};
}
