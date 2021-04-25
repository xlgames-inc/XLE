// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowPreparer.h"
#include "LightDesc.h"
#include "ShadowUniforms.h"
#include "RenderStepFragments.h"
#include "SharedTechniqueDelegates.h"
#include "../Techniques/PipelineAccelerator.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/TechniqueDelegates.h"
#include "../Techniques/RenderStateResolver.h"
#include <vector>

namespace RenderCore { namespace LightingEngine
{
	class CompiledShadowPreparer : public ICompiledShadowPreparer
	{
	public:
		Techniques::RenderPassInstance Begin(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			const ShadowProjectionDesc& frustum,
			Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			Techniques::AttachmentPool& shadowGenAttachmentPool) override;

		PreparedShadowFrustum End(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			Techniques::RenderPassInstance& rpi) override;

		std::pair<std::shared_ptr<Techniques::SequencerConfig>, std::shared_ptr<Techniques::IShaderResourceDelegate>> GetSequencerConfig() override;

		CompiledShadowPreparer(
			const ShadowGeneratorDesc& desc,
			const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox);
		~CompiledShadowPreparer();

	private:
		std::shared_ptr<Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		FrameBufferDesc _fbDesc;
		std::shared_ptr<Techniques::SequencerConfig> _sequencerConfigs;
		std::shared_ptr<Techniques::IShaderResourceDelegate> _uniformDelegate;

		Techniques::ProjectionDesc _savedProjectionDesc;

		PreparedDMShadowFrustum _workingDMFrustum;

		class UniformDelegate : public Techniques::IShaderResourceDelegate
		{
		public:
			virtual const UniformsStreamInterface& GetInterface() override { return _interface; }
			void WriteImmediateData(Techniques::ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst) override
			{
				switch (idx) {
				case 0:
					assert(dst.size() == sizeof(CB_ArbitraryShadowProjection)); 
					std::memcpy(dst.begin(), &_preparer->_workingDMFrustum._arbitraryCBSource, sizeof(CB_ArbitraryShadowProjection));
					break;
				case 1:
					assert(dst.size() == sizeof(CB_OrthoShadowProjection)); 
					std::memcpy(dst.begin(), &_preparer->_workingDMFrustum._orthoCBSource, sizeof(CB_OrthoShadowProjection));
					break;
				default:
					assert(0);
					break;
				}
			}

			size_t GetImmediateDataSize(Techniques::ParsingContext& context, const void* objectContext, unsigned idx) override
			{
				switch (idx) {
				case 0: return sizeof(CB_ArbitraryShadowProjection);
				case 1: return sizeof(CB_OrthoShadowProjection);
				default:
					assert(0);
					return 0;
				}
			}
		
			UniformDelegate(CompiledShadowPreparer& preparer) : _preparer(&preparer)
			{
				_interface.BindImmediateData(0, Utility::Hash64("ArbitraryShadowProjection"), {});
				_interface.BindImmediateData(1, Utility::Hash64("OrthogonalShadowProjection"), {});
			}
			UniformsStreamInterface _interface;
			CompiledShadowPreparer* _preparer;
		};
	};

	ICompiledShadowPreparer::~ICompiledShadowPreparer() {}

	Techniques::RenderPassInstance CompiledShadowPreparer::Begin(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		const ShadowProjectionDesc& frustum,
		Techniques::FrameBufferPool& shadowGenFrameBufferPool,
		Techniques::AttachmentPool& shadowGenAttachmentPool)
	{
		_workingDMFrustum = SetupPreparedDMShadowFrustum(frustum);
		assert(_workingDMFrustum.IsReady());
		assert(!_fbDesc.GetSubpasses().empty());
		_savedProjectionDesc = parsingContext.GetProjectionDesc();
		parsingContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
		return Techniques::RenderPassInstance{threadContext, _fbDesc, shadowGenFrameBufferPool, shadowGenAttachmentPool};
	}

	PreparedShadowFrustum CompiledShadowPreparer::End(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		Techniques::RenderPassInstance& rpi)
	{
		// todo -- create a uniform delegate and attach it to the parsing context
		/*
		if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenDebugging", 0)) {
			auto srvForDebugging = *rpi.GetRenderPassInstance().GetDepthStencilAttachmentSRV(TextureViewDesc{TextureViewDesc::Aspect::ColorLinear});
			parsingContext._pendingOverlays.push_back(
				std::bind(
					&ShadowGen_DrawDebugging, 
					std::placeholders::_1, std::placeholders::_2,
					srvForDebugging));
		}

		if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenFrustumDebugging", 0)) {
			parsingContext._pendingOverlays.push_back(
				std::bind(
					&ShadowGen_DrawShadowFrustums, 
					std::placeholders::_1, std::placeholders::_2,
					lightingParserContext.GetMainTargets(),
					shadowDelegate._shadowProj));
		}
		*/

		parsingContext.GetProjectionDesc() = _savedProjectionDesc;
		return std::move(_workingDMFrustum);
	}

	std::pair<std::shared_ptr<Techniques::SequencerConfig>, std::shared_ptr<Techniques::IShaderResourceDelegate>> CompiledShadowPreparer::GetSequencerConfig()
	{
		return std::make_pair(_sequencerConfigs, _uniformDelegate);
	}

	static const auto s_shadowCascadeModeString = "SHADOW_CASCADE_MODE";
    static const auto s_shadowEnableNearCascadeString = "SHADOW_ENABLE_NEAR_CASCADE";

	CompiledShadowPreparer::CompiledShadowPreparer(
		const ShadowGeneratorDesc& desc,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox)
	: _pipelineAccelerators(pipelineAccelerators)
	{
		assert(desc._resolveType == ShadowResolveType::DepthTexture);

		///////////////////////////////
		RenderStepFragmentInterface fragment{PipelineType::Graphics};
		{
			Techniques::RSDepthBias singleSidedBias {
				desc._rasterDepthBias, desc._depthBiasClamp, desc._slopeScaledBias };
			Techniques::RSDepthBias doubleSidedBias {
				desc._dsRasterDepthBias, desc._dsDepthBiasClamp, desc._dsSlopeScaledBias };

			auto output = fragment.DefineAttachment(
				Techniques::AttachmentSemantics::ShadowDepthMap, 
				{
					AsTypelessFormat(desc._format),
					float(desc._width), float(desc._height),
					desc._arrayCount 
				});
			
			auto shadowGenDelegate = delegatesBox->GetShadowGenTechniqueDelegate(singleSidedBias, doubleSidedBias, desc._cullMode);

			ParameterBox box;
			box.SetParameter(s_shadowCascadeModeString, desc._projectionMode == ShadowProjectionMode::Ortho?2:1);
			box.SetParameter(s_shadowEnableNearCascadeString, desc._enableNearCascade?1:0);

			SubpassDesc subpass;
			subpass.SetDepthStencil(output, LoadStore::Clear, LoadStore::Retain);
			fragment.AddSubpass(
				std::move(subpass),
				shadowGenDelegate,
				Techniques::BatchFilter::Max,
				std::move(box));
		}
		///////////////////////////////
		
		auto merged = Techniques::MergeFragments(
			{}, MakeIteratorRange(&fragment.GetFrameBufferDescFragment(), &fragment.GetFrameBufferDescFragment()+1));
		_fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment), FrameBufferProperties{});

		_sequencerConfigs = pipelineAccelerators->CreateSequencerConfig(
			fragment.GetSubpassAddendums()[0]._techniqueDelegate,
			fragment.GetSubpassAddendums()[0]._sequencerSelectors,
			_fbDesc,
			0);
		_uniformDelegate = std::make_shared<UniformDelegate>(*this);
	}

	CompiledShadowPreparer::~CompiledShadowPreparer() {}

	std::shared_ptr<ICompiledShadowPreparer> CreateCompiledShadowPreparer(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerator,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox)
	{
		return std::make_shared<CompiledShadowPreparer>(desc, pipelineAccelerator, delegatesBox);
	}


	template<int BitCount, typename Input>
		static uint64_t GetBits(Input i)
	{
		auto mask = (1ull<<uint64_t(BitCount))-1ull;
		assert((uint64_t(i) & ~mask) == 0);
		return uint64_t(i) & mask;
	}

	inline uint32_t FloatBits(float i) { return *(uint32_t*)&i; }

	uint64_t Hash64(const ShadowGeneratorDesc& shadowGeneratorDesc, uint64_t seed)
	{
		uint64_t h0 = 
			  (GetBits<12>(shadowGeneratorDesc._width)			<< 0ull)
			| (GetBits<12>(shadowGeneratorDesc._height)			<< 12ull)
			| (GetBits<8>(shadowGeneratorDesc._format)			<< 24ull)
			| (GetBits<4>(shadowGeneratorDesc._arrayCount)		<< 32ull)
			| (GetBits<4>(shadowGeneratorDesc._projectionMode)	<< 36ull)
			| (GetBits<4>(shadowGeneratorDesc._cullMode)		<< 40ull)
			| (GetBits<4>(shadowGeneratorDesc._resolveType)		<< 44ull)
			| (GetBits<1>(shadowGeneratorDesc._enableNearCascade)  << 48ull)
			;

		uint64_t h1 = 
				uint64_t(FloatBits(shadowGeneratorDesc._slopeScaledBias))
			|  (uint64_t(FloatBits(shadowGeneratorDesc._depthBiasClamp)) << 32ull)
			;

		uint64_t h2 = 
				uint64_t(FloatBits(shadowGeneratorDesc._dsSlopeScaledBias))
			|  (uint64_t(FloatBits(shadowGeneratorDesc._dsDepthBiasClamp)) << 32ull)
			;

		uint64_t h3 = 
				uint64_t(shadowGeneratorDesc._rasterDepthBias)
			|  (uint64_t(shadowGeneratorDesc._dsRasterDepthBias) << 32ull)
			;

		return HashCombine(h0, HashCombine(h1, HashCombine(h2, HashCombine(h3, seed))));
	}

}}

