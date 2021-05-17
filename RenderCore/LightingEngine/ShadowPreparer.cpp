// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowPreparer.h"
#include "LightDesc.h"
#include "ShadowUniforms.h"
#include "RenderStepFragments.h"
#include "LightingEngineApparatus.h"
#include "../Techniques/PipelineAccelerator.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/TechniqueDelegates.h"
#include "../Techniques/RenderStateResolver.h"
#include "../Techniques/DrawableDelegates.h"
#include "../Techniques/CommonBindings.h"
#include "../Assets/PredefinedDescriptorSetLayout.h"
#include "../IDevice.h"
#include "../IThreadContext.h"
#include "../../Assets/AssetFuture.h"
#include <vector>

namespace RenderCore { namespace LightingEngine
{
	class PreparedShadowResult : public IPreparedShadowResult
	{
	public:
		std::shared_ptr<IDescriptorSet> _descriptorSet;
		virtual const std::shared_ptr<IDescriptorSet>& GetDescriptorSet() const override { return _descriptorSet; }
		virtual ~PreparedShadowResult() {}
	};

	IPreparedShadowResult::~IPreparedShadowResult() {}

	class DMShadowPreparer : public ICompiledShadowPreparer
	{
	public:
		Techniques::RenderPassInstance Begin(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			const ShadowProjectionDesc& frustum,
			Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			Techniques::AttachmentPool& shadowGenAttachmentPool) override;

		void End(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			Techniques::RenderPassInstance& rpi,
			IPreparedShadowResult& res) override;

		std::pair<std::shared_ptr<Techniques::SequencerConfig>, std::shared_ptr<Techniques::IShaderResourceDelegate>> GetSequencerConfig() override;
		std::shared_ptr<IPreparedShadowResult> CreatePreparedShadowResult() override;

		DMShadowPreparer(
			const ShadowGeneratorDesc& desc,
			const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
			const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout);
		~DMShadowPreparer();

	private:
		std::shared_ptr<Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		Techniques::FragmentStitchingContext::StitchResult _fbDesc;
		std::shared_ptr<Techniques::SequencerConfig> _sequencerConfigs;
		std::shared_ptr<Techniques::IShaderResourceDelegate> _uniformDelegate;

		Techniques::ProjectionDesc _savedProjectionDesc;

		PreparedDMShadowFrustum _workingDMFrustum;

		DescriptorSetSignature _descSetSig;
		std::vector<DescriptorSetInitializer::BindTypeAndIdx> _descSetSlotBindings;

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
		
			UniformDelegate(DMShadowPreparer& preparer) : _preparer(&preparer)
			{
				_interface.BindImmediateData(0, Utility::Hash64("ArbitraryShadowProjection"), {});
				_interface.BindImmediateData(1, Utility::Hash64("OrthogonalShadowProjection"), {});
			}
			UniformsStreamInterface _interface;
			DMShadowPreparer* _preparer;
		};
	};

	ICompiledShadowPreparer::~ICompiledShadowPreparer() {}

	Techniques::RenderPassInstance DMShadowPreparer::Begin(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		const ShadowProjectionDesc& frustum,
		Techniques::FrameBufferPool& shadowGenFrameBufferPool,
		Techniques::AttachmentPool& shadowGenAttachmentPool)
	{
		_workingDMFrustum = SetupPreparedDMShadowFrustum(frustum);
		assert(_workingDMFrustum.IsReady());
		assert(!_fbDesc._fbDesc.GetSubpasses().empty());
		_savedProjectionDesc = parsingContext.GetProjectionDesc();
		parsingContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
		return Techniques::RenderPassInstance{
			threadContext,
			_fbDesc._fbDesc, _fbDesc._fullAttachmentDescriptions,
			shadowGenFrameBufferPool, shadowGenAttachmentPool, {}};
	}

	void DMShadowPreparer::End(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		Techniques::RenderPassInstance& rpi,
		IPreparedShadowResult& res)
	{
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

		auto& device = *threadContext.GetDevice();
		DescriptorSetInitializer descSetInit;
		descSetInit._signature = &_descSetSig;
		descSetInit._slotBindings = _descSetSlotBindings;
		const IResourceView* srvs[] = { rpi.GetDepthStencilAttachmentSRV({}) };
		IteratorRange<const void*> immediateData[3];
		if (_workingDMFrustum._mode == ShadowProjectionMode::Arbitrary) {
			immediateData[0] = MakeOpaqueIteratorRange(_workingDMFrustum._arbitraryCBSource);
		} else {
			immediateData[0] = MakeOpaqueIteratorRange(_workingDMFrustum._orthoCBSource);
		}
		immediateData[1] = MakeOpaqueIteratorRange(_workingDMFrustum._resolveParameters);
		auto screenToShadow = BuildScreenToShadowProjection(
			_workingDMFrustum._frustumCount,
			_workingDMFrustum._arbitraryCBSource,
			_workingDMFrustum._orthoCBSource,
			_savedProjectionDesc._cameraToWorld,
			_savedProjectionDesc._cameraToProjection);
		immediateData[2] = MakeOpaqueIteratorRange(screenToShadow);
		descSetInit._bindItems._resourceViews = MakeIteratorRange(srvs);
		descSetInit._bindItems._immediateData = MakeIteratorRange(immediateData);
		auto descSet = device.CreateDescriptorSet(descSetInit);
		checked_cast<PreparedShadowResult*>(&res)->_descriptorSet = std::move(descSet);

		parsingContext.GetProjectionDesc() = _savedProjectionDesc;
	}

	std::pair<std::shared_ptr<Techniques::SequencerConfig>, std::shared_ptr<Techniques::IShaderResourceDelegate>> DMShadowPreparer::GetSequencerConfig()
	{
		return std::make_pair(_sequencerConfigs, _uniformDelegate);
	}

	std::shared_ptr<IPreparedShadowResult> DMShadowPreparer::CreatePreparedShadowResult()
	{
		return std::make_shared<PreparedShadowResult>();
	}

	static const auto s_shadowCascadeModeString = "SHADOW_CASCADE_MODE";
    static const auto s_shadowEnableNearCascadeString = "SHADOW_ENABLE_NEAR_CASCADE";

	DMShadowPreparer::DMShadowPreparer(
		const ShadowGeneratorDesc& desc,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
		const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout)
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
				desc._width, desc._height, desc._arrayCount,
				AttachmentDesc{desc._format, 0, LoadStore::Clear, LoadStore::Retain, 0, BindFlag::ShaderResource});
			
			auto shadowGenDelegate = delegatesBox->GetShadowGenTechniqueDelegate(singleSidedBias, doubleSidedBias, desc._cullMode);

			ParameterBox box;
			box.SetParameter(s_shadowCascadeModeString, desc._projectionMode == ShadowProjectionMode::Ortho?2:1);
			box.SetParameter(s_shadowEnableNearCascadeString, desc._enableNearCascade?1:0);

			SubpassDesc subpass;
			subpass.SetDepthStencil(output);
			fragment.AddSubpass(
				std::move(subpass),
				shadowGenDelegate,
				Techniques::BatchFilter::Max,
				std::move(box));
		}
		///////////////////////////////
		
		Techniques::FragmentStitchingContext stitchingContext;
		stitchingContext._workingProps = FrameBufferProperties { desc._width, desc._height };
		_fbDesc = stitchingContext.TryStitchFrameBufferDesc(fragment.GetFrameBufferDescFragment());

		_sequencerConfigs = pipelineAccelerators->CreateSequencerConfig(
			fragment.GetSubpassAddendums()[0]._techniqueDelegate,
			fragment.GetSubpassAddendums()[0]._sequencerSelectors,
			_fbDesc._fbDesc,
			0);
		_uniformDelegate = std::make_shared<UniformDelegate>(*this);

		if (descSetLayout) {
			_descSetSig = descSetLayout->MakeDescriptorSetSignature();
			_descSetSlotBindings.reserve(descSetLayout->_slots.size());
			for (const auto& s:descSetLayout->_slots) {
				if (s._name == "DMShadow") {
					_descSetSlotBindings.push_back({DescriptorSetInitializer::BindType::ResourceView, 0});
				} else if (s._name == "ShadowProjection") {
					_descSetSlotBindings.push_back({DescriptorSetInitializer::BindType::ImmediateData, 0});
				} else if (s._name == "ShadowResolveParameters") {
					_descSetSlotBindings.push_back({DescriptorSetInitializer::BindType::ImmediateData, 1});
				} else if (s._name == "ScreenToShadowProjection") {
					_descSetSlotBindings.push_back({DescriptorSetInitializer::BindType::ImmediateData, 2});
				} else 
					_descSetSlotBindings.push_back({});
			}
		}
	}

	DMShadowPreparer::~DMShadowPreparer() {}

	::Assets::FuturePtr<ICompiledShadowPreparer> CreateCompiledShadowPreparer(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
		const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout)
	{
		auto result = std::make_shared<::Assets::AssetFuture<ICompiledShadowPreparer>>();
		result->SetAsset(std::make_shared<DMShadowPreparer>(desc, pipelineAccelerators, delegatesBox, descSetLayout), nullptr);
		return result;
	}

	::Assets::FuturePtr<ShadowPreparationOperators> CreateShadowPreparationOperators(
		IteratorRange<const ShadowGeneratorDesc*> shadowGenerators, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox,
		const std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout>& descSetLayout)
	{
		using PreparerFuture = ::Assets::FuturePtr<ICompiledShadowPreparer>;
		std::vector<PreparerFuture> futures;
		futures.reserve(shadowGenerators.size());
		for (const auto&desc:shadowGenerators)
			futures.push_back(CreateCompiledShadowPreparer(desc, pipelineAccelerators, delegatesBox, descSetLayout));

		auto result = std::make_shared<::Assets::AssetFuture<ShadowPreparationOperators>>();
		result->SetPollingFunction(
			[futures=std::move(futures)](::Assets::AssetFuture<ShadowPreparationOperators>& future) -> bool {
				using namespace ::Assets;
				std::vector<std::shared_ptr<ICompiledShadowPreparer>> actualized;
				actualized.resize(futures.size());
				auto a=actualized.begin();
				for (const auto& p:futures) {
					Blob queriedLog;
					DependencyValidation queriedDepVal;
					auto state = p->CheckStatusBkgrnd(*a, queriedDepVal, queriedLog);
					if (state != AssetState::Ready) {
						if (state == AssetState::Invalid) {
							future.SetInvalidAsset(queriedDepVal, queriedLog);
							return false;
						} else 
							return true;
					}
					++a;
				}

				auto finalResult = std::make_shared<ShadowPreparationOperators>();
				finalResult->_operators.reserve(actualized.size());
				for (auto& a:actualized)
					finalResult->_operators.push_back({std::move(a), ShadowGeneratorDesc{}});

				future.SetAsset(std::move(finalResult), nullptr);
				return false;
			});
		return result;
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

