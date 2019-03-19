// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "SceneParser.h"
#include "LightDesc.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/RenderPass.h"
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
		virtual RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch) = 0;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class RenderStep_Forward : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate();
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_Forward(bool precisionTargets);
		~RenderStep_Forward();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _forward;
	};

	class RenderStep_Direct : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate();
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_Direct();
		~RenderStep_Direct();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _direct;
	};

	class RenderStep_GBuffer : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate();
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_GBuffer(unsigned gbufferType, bool precisionTargets);
		~RenderStep_GBuffer();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _createGBuffer;
		unsigned _gbufferType;
	};

	class RenderStep_PrepareDMShadows : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderCore::IResourcePtr _resource;

		RenderStep_PrepareDMShadows(RenderCore::Format format, UInt2 dims, unsigned projectionCount);
		~RenderStep_PrepareDMShadows();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class RenderStep_PrepareRTShadows : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_PrepareRTShadows();
		~RenderStep_PrepareRTShadows();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class ViewDelegate_Shadow : public IViewDelegate
	{
	public:
		RenderCore::Techniques::DrawablesPacket _general;
		ShadowProjectionDesc _shadowProj;

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch);
		ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection);
		~ViewDelegate_Shadow();
	};

	class RenderStep_LightingResolve : public IRenderStep
	{
	public:
		virtual const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const { return _fragment; }
		virtual void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_LightingResolve(bool precisionTargets);
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class RenderStep_ResolveHDR : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IViewDelegate* viewDelegate);

		RenderStep_ResolveHDR();
		~RenderStep_ResolveHDR();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};


}
