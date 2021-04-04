// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueDelegates.h"
#include "CommonResources.h"
#include "../Assets/MaterialScaffold.h"
#include "../IDevice.h"
#include "../Format.h"
#include "../../ShaderParser/AutomaticSelectorFiltering.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../xleres/FileList.h"
#include <sstream>
#include <regex>
#include <cctype>
#include <charconv>

namespace RenderCore { namespace Techniques
{
	class TechniqueSharedResources
	{
	public:
		CommonResourceBox _commonResources;
		TechniqueSharedResources(IDevice& device) : _commonResources(device) {}
		~TechniqueSharedResources() {}
	};

	std::shared_ptr<TechniqueSharedResources> MakeTechniqueSharedResources(IDevice& device)
	{
		return std::make_shared<TechniqueSharedResources>(device);
	}

#if 0
	class ShaderPatchFactory : public IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<CompiledShaderByteCode> MakeByteCodeFuture(
			ShaderStage stage, StringSection<> initializer, StringSection<> definesTable)
		{
			assert(!initializer.IsEmpty());

			char temp[MaxPath];
			auto meld = StringMeldInPlace(temp);
			meld << initializer;

			// shader profile
			{
				char profileStr[] = "?s_";
				switch (stage) {
				case ShaderStage::Vertex: profileStr[0] = 'v'; break;
				case ShaderStage::Geometry: profileStr[0] = 'g'; break;
				case ShaderStage::Pixel: profileStr[0] = 'p'; break;
				case ShaderStage::Domain: profileStr[0] = 'd'; break;
				case ShaderStage::Hull: profileStr[0] = 'h'; break;
				case ShaderStage::Compute: profileStr[0] = 'c'; break;
				default: assert(0); break;
				}
				if (!XlFindStringI(initializer, profileStr)) {
					meld << ":" << profileStr << "*";
				}
			}

			auto ret = ::Assets::MakeAsset<CompiledShaderByteCode_InstantiateShaderGraph>(MakeStringSection(temp), definesTable, _patchCollection, _patchExpansions);
			return std::reinterpret_pointer_cast<::Assets::AssetFuture<CompiledShaderByteCode>>(ret);
		}

		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines)
		{
			::Assets::FuturePtr<CompiledShaderByteCode> vsCode, psCode, gsCode;
			vsCode = MakeByteCodeFuture(ShaderStage::Vertex, _entry->_vertexShaderName, defines);
			psCode = MakeByteCodeFuture(ShaderStage::Pixel, _entry->_pixelShaderName, defines);

			if (!_entry->_geometryShaderName.empty()) {
				auto finalDefines = defines.AsString() + _soExtraDefines;
				gsCode = MakeByteCodeFuture(ShaderStage::Geometry, _entry->_geometryShaderName, finalDefines);

				return CreateShaderProgramFromByteCode(
					_pipelineLayout,
					vsCode, gsCode, psCode,
					StreamOutputInitializers {
						MakeIteratorRange(_soElements), MakeIteratorRange(_soStrides)
					},
					"ShaderPatchFactory");
			} else {
				return CreateShaderProgramFromByteCode(_pipelineLayout, vsCode, psCode, "ShaderPatchFactory");
			}
		}

		ShaderPatchFactory(
			const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
			const TechniqueEntry& techEntry, 
			const std::shared_ptr<CompiledShaderPatchCollection>& patchCollection,
			IteratorRange<const uint64_t*> patchExpansions,
			const StreamOutputInitializers& so = {})
		: _entry(&techEntry)
		, _patchCollection(patchCollection)
		, _patchExpansions(patchExpansions.begin(), patchExpansions.end())
		, _pipelineLayout(pipelineLayout)
		{
			_factoryGuid = _patchCollection ? _patchCollection->GetGUID() : 0;

			if (!so._outputElements.empty() && !so._outputBufferStrides.empty()) {
				std::stringstream str;
				str << ";SO_OFFSETS=";
				unsigned rollingOffset = 0;
				for (const auto&e:so._outputElements) {
					assert(e._alignedByteOffset == ~0x0u);		// expecting to use packed sequential ordering
					if (rollingOffset!=0) str << ",";
					str << Hash64(e._semanticName) + e._semanticIndex << "," << rollingOffset;
					rollingOffset += BitsPerPixel(e._nativeFormat) / 8;
				}
				_soExtraDefines = str.str();
				_soElements = std::vector<InputElementDesc>(so._outputElements.begin(), so._outputElements.end());
				_soStrides = std::vector<unsigned>(so._outputBufferStrides.begin(), so._outputBufferStrides.end());

				_factoryGuid = HashCombine(Hash64(_soExtraDefines), _factoryGuid);
				_factoryGuid = HashCombine(Hash64(so._outputBufferStrides.begin(), so._outputBufferStrides.end()), _factoryGuid);
			}
		}
		ShaderPatchFactory() {}
	private:
		const TechniqueEntry* _entry;
		std::shared_ptr<CompiledShaderPatchCollection> _patchCollection;
		std::vector<uint64_t> _patchExpansions;
		std::shared_ptr<ICompiledPipelineLayout> _pipelineLayout;

		std::string _soExtraDefines;
		std::vector<InputElementDesc> _soElements;
		std::vector<unsigned> _soStrides;
	};
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_Legacy : public ITechniqueDelegate
	{
	public:
		::Assets::FuturePtr<GraphicsPipelineDesc> Resolve(
			const CompiledShaderPatchCollection::Interface& shaderPatches,
			const RenderCore::Assets::RenderStateSet& input) override;

		TechniqueDelegate_Legacy(
			unsigned techniqueIndex,
			const AttachmentBlendDesc& blend,
			const RasterizationDesc& rasterization,
			const DepthStencilDesc& depthStencil);
		~TechniqueDelegate_Legacy();
	private:
		unsigned _techniqueIndex;
		AttachmentBlendDesc _blend;
		RasterizationDesc _rasterization;
		DepthStencilDesc _depthStencil;
		::Assets::FuturePtr<Technique> _techniqueFuture;
	};

	static void PrepareShadersFromTechniqueEntry(
		::Assets::AssetFuture<ITechniqueDelegate::GraphicsPipelineDesc>& future,
		const std::shared_ptr<ITechniqueDelegate::GraphicsPipelineDesc>& nascentDesc,
		const TechniqueEntry& entry)
	{
		nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer = entry._vertexShaderName;
		nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer = entry._pixelShaderName;
		nascentDesc->_shaders[(unsigned)ShaderStage::Geometry]._initializer = entry._geometryShaderName;
		nascentDesc->_manualSelectorFiltering = entry._selectorFiltering;

		auto vsfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer).AllExceptParameters();
		auto psfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer).AllExceptParameters();

		auto vsFilteringFuture = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(vsfn);
		auto psFilteringFuture = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(psfn);
		if (entry._geometryShaderName.empty()) {
			::Assets::WhenAll(vsFilteringFuture, psFilteringFuture).ThenConstructToFuture<ITechniqueDelegate::GraphicsPipelineDesc>(
				future,
				[nascentDesc](
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& vsFiltering,
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& psFiltering) {
					
					nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._automaticFiltering = vsFiltering;
					nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._automaticFiltering = psFiltering;
					return nascentDesc;
				});
		} else {
			auto gsfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Geometry]._initializer).AllExceptParameters();
			auto gsFilteringFuture = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(gsfn);
			::Assets::WhenAll(vsFilteringFuture, psFilteringFuture, gsFilteringFuture).ThenConstructToFuture<ITechniqueDelegate::GraphicsPipelineDesc>(
				future,
				[nascentDesc](
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& vsFiltering,
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& psFiltering,
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& gsFiltering) {
					
					nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._automaticFiltering = vsFiltering;
					nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._automaticFiltering = psFiltering;
					nascentDesc->_shaders[(unsigned)ShaderStage::Geometry]._automaticFiltering = gsFiltering;
					return nascentDesc;
				});
		}
	}

	auto TechniqueDelegate_Legacy::Resolve(
		const CompiledShaderPatchCollection::Interface& shaderPatches,
		const RenderCore::Assets::RenderStateSet& input) -> ::Assets::FuturePtr<GraphicsPipelineDesc>
	{
		auto result = std::make_shared<::Assets::AssetFuture<GraphicsPipelineDesc>>("resolved-technique");

		auto nascentDesc = std::make_shared<GraphicsPipelineDesc>();
		nascentDesc->_blend.push_back(_blend);
		nascentDesc->_rasterization = _rasterization;
		nascentDesc->_depthStencil = _depthStencil;

		auto technique = _techniqueFuture->TryActualize();
		if (technique) {
			nascentDesc->_depVal = technique->GetDependencyValidation();
			auto& entry = technique->GetEntry(_techniqueIndex);
			PrepareShadersFromTechniqueEntry(*result, nascentDesc, entry);
		} else {
			// We need to poll until the technique file is ready, and then continue on to figuring out the shader
			// information as usual
			::Assets::WhenAll(_techniqueFuture).ThenConstructToFuture<GraphicsPipelineDesc>(
				*result,
				[techniqueIndex = _techniqueIndex, nascentDesc](
					::Assets::AssetFuture<GraphicsPipelineDesc>& thatFuture,
					const std::shared_ptr<Technique>& technique) {

					nascentDesc->_depVal = technique->GetDependencyValidation();
					auto& entry = technique->GetEntry(techniqueIndex);
					PrepareShadersFromTechniqueEntry(thatFuture, nascentDesc, entry);
				});
		}

		return result;
	}

	TechniqueDelegate_Legacy::TechniqueDelegate_Legacy(
		unsigned techniqueIndex,
		const AttachmentBlendDesc& blend,
		const RasterizationDesc& rasterization,
		const DepthStencilDesc& depthStencil)
	: _techniqueIndex(techniqueIndex)
	, _blend(blend)
	, _rasterization(rasterization)
	, _depthStencil(depthStencil)
	{
		const char* techFile = ILLUM_LEGACY_TECH;
		_techniqueFuture = ::Assets::MakeAsset<Technique>(techFile);
	}

	TechniqueDelegate_Legacy::~TechniqueDelegate_Legacy()
	{
	}

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegateLegacy(
		unsigned techniqueIndex,
		const AttachmentBlendDesc& blend,
		const RasterizationDesc& rasterization,
		const DepthStencilDesc& depthStencil)
	{
		return std::make_shared<TechniqueDelegate_Legacy>(techniqueIndex, blend, rasterization, depthStencil);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		T E C H N I Q U E   D E L E G A T E
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
	class TechniqueDelegate_Base : public ITechniqueDelegate
	{
	protected:
		::Assets::FuturePtr<Metal::ShaderProgram> ResolveVariation(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const TechniqueEntry& techEntry,
			IteratorRange<const uint64_t*> patchExpansions)
		{
			/*StreamOutputInitializers soInit;
			soInit._outputElements = MakeIteratorRange(_soElements);
			soInit._outputBufferStrides = MakeIteratorRange(_soStrides);

			ShaderPatchFactory factory(nullptr, techEntry, shaderPatches, patchExpansions, soInit);
			const auto& variation = _sharedResources->_mainVariationSet.FindVariation(
				selectors,
				techEntry._selectorFiltering,
				shaderPatches->GetInterface().GetSelectorRelevance(),
				factory);
			return variation._shaderFuture;*/
			assert(0);
			return nullptr;
		}

		std::shared_ptr<TechniqueSharedResources> _sharedResources;
		std::vector<InputElementDesc> _soElements;
		std::vector<unsigned> _soStrides;
	};
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static const auto s_perPixel = Hash64("PerPixel");
	static const auto s_earlyRejectionTest = Hash64("EarlyRejectionTest");
	static const auto s_deformVertex = Hash64("DeformVertex");
	static uint64_t s_patchExp_perPixelAndEarlyRejection[] = { s_perPixel, s_earlyRejectionTest };
	static uint64_t s_patchExp_perPixel[] = { s_perPixel };
	static uint64_t s_patchExp_earlyRejection[] = { s_earlyRejectionTest };

	IllumType CalculateIllumType(const CompiledShaderPatchCollection::Interface& shaderPatches)
	{
		if (shaderPatches.HasPatchType(s_perPixel)) {
			if (shaderPatches.HasPatchType(s_earlyRejectionTest)) {
				return IllumType::PerPixelAndEarlyRejection;
			} else {
				return IllumType::PerPixel;
			}
		}
		return IllumType::NoPerPixel;
	}

	class TechniqueDelegate_Deferred : public ITechniqueDelegate
	{
	public:
		struct TechniqueFileHelper
		{
		public:
			std::shared_ptr<TechniqueSetFile> _techniqueSet;
			TechniqueEntry _noPatches;
			TechniqueEntry _perPixel;
			TechniqueEntry _perPixelAndEarlyRejection;
			TechniqueEntry _vsNoPatchesSrc;
			TechniqueEntry _vsDeformVertexSrc;

			const ::Assets::DepValPtr& GetDependencyValidation() const { return _techniqueSet->GetDependencyValidation(); }

			TechniqueFileHelper(const std::shared_ptr<TechniqueSetFile>& techniqueSet)
			: _techniqueSet(techniqueSet)
			{
				const auto noPatchesHash = Hash64("Deferred_NoPatches");
				const auto perPixelHash = Hash64("Deferred_PerPixel");
				const auto perPixelAndEarlyRejectionHash = Hash64("Deferred_PerPixelAndEarlyRejection");
				const auto vsNoPatchesHash = Hash64("VS_NoPatches");
				const auto vsDeformVertexHash = Hash64("VS_DeformVertex");
				auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
				auto* perPixelSrc = _techniqueSet->FindEntry(perPixelHash);
				auto* perPixelAndEarlyRejectionSrc = _techniqueSet->FindEntry(perPixelAndEarlyRejectionHash);
				auto* vsNoPatchesSrc = _techniqueSet->FindEntry(vsNoPatchesHash);
				auto* vsDeformVertexSrc = _techniqueSet->FindEntry(vsDeformVertexHash);
				if (!noPatchesSrc || !perPixelSrc || !perPixelAndEarlyRejectionSrc || !vsNoPatchesSrc || !vsDeformVertexSrc) {
					Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
				}

				_noPatches = *noPatchesSrc;
				_perPixel = *perPixelSrc;
				_perPixelAndEarlyRejection = *perPixelAndEarlyRejectionSrc;
				_vsNoPatchesSrc = *vsNoPatchesSrc;
				_vsDeformVertexSrc = *vsDeformVertexSrc;
			}
		};

		::Assets::FuturePtr<GraphicsPipelineDesc> Resolve(
			const CompiledShaderPatchCollection::Interface& shaderPatches,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			auto result = std::make_shared<::Assets::AssetFuture<GraphicsPipelineDesc>>("from-deferred-delegate");

			auto nascentDesc = std::make_shared<GraphicsPipelineDesc>();
			nascentDesc->_rasterization = BuildDefaultRastizerDesc(stateSet);
			bool deferredDecal = 
					(stateSet._flag & Assets::RenderStateSet::Flag::BlendType)
				&&	(stateSet._blendType == Assets::RenderStateSet::BlendType::DeferredDecal);
			nascentDesc->_blend.push_back(deferredDecal ? CommonResourceBox::s_abStraightAlpha : CommonResourceBox::s_abOpaque);

			auto illumType = CalculateIllumType(shaderPatches);
			bool hasDeformVertex = shaderPatches.HasPatchType(s_deformVertex);

			::Assets::WhenAll(_techniqueFileHelper).ThenConstructToFuture<GraphicsPipelineDesc>(
				*result,
				[nascentDesc, illumType, hasDeformVertex](
					::Assets::AssetFuture<GraphicsPipelineDesc>& thatFuture,
					const std::shared_ptr<TechniqueFileHelper>& techniqueFileHelper) {

					nascentDesc->_depVal = techniqueFileHelper->GetDependencyValidation();

					const TechniqueEntry* psTechEntry = &techniqueFileHelper->_noPatches;
					switch (illumType) {
					case IllumType::PerPixel:
						psTechEntry = &techniqueFileHelper->_perPixel;
						nascentDesc->_patchExpansions.insert(nascentDesc->_patchExpansions.end(), s_patchExp_perPixel, &s_patchExp_perPixel[dimof(s_patchExp_perPixel)]);
						break;
					case IllumType::PerPixelAndEarlyRejection:
						psTechEntry = &techniqueFileHelper->_perPixelAndEarlyRejection;
						nascentDesc->_patchExpansions.insert(nascentDesc->_patchExpansions.end(), s_patchExp_perPixelAndEarlyRejection, &s_patchExp_perPixelAndEarlyRejection[dimof(s_patchExp_perPixelAndEarlyRejection)]);
						break;
					default:
						break;
					}

					const TechniqueEntry* vsTechEntry = &techniqueFileHelper->_vsNoPatchesSrc;
					if (hasDeformVertex) {
						vsTechEntry = &techniqueFileHelper->_vsDeformVertexSrc;
						nascentDesc->_patchExpansions.push_back(s_deformVertex);
					}
					
					// note -- we could premerge all of the combinations in the constructor, to cut down on cost here
					TechniqueEntry mergedTechEntry = *vsTechEntry;
					mergedTechEntry.MergeIn(*psTechEntry);

					PrepareShadersFromTechniqueEntry(thatFuture, nascentDesc, mergedTechEntry);
				});
			
			return result;
		}

		TechniqueDelegate_Deferred(
			const ::Assets::FuturePtr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources)
		: _sharedResources(sharedResources)
		{
			_techniqueFileHelper = std::make_shared<::Assets::AssetFuture<TechniqueFileHelper>>("");
			::Assets::WhenAll(techniqueSet).ThenConstructToFuture<TechniqueFileHelper>(*_techniqueFileHelper);
		}
	private:
		::Assets::FuturePtr<TechniqueFileHelper> _techniqueFileHelper;
		std::shared_ptr<TechniqueSharedResources> _sharedResources;
	};

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_Deferred(
		const ::Assets::FuturePtr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	{
		return std::make_shared<TechniqueDelegate_Deferred>(techniqueSet, sharedResources);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
	class TechniqueDelegate_Forward : public TechniqueDelegate_Base
	{
	public:
		GraphicsPipelineDesc Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			std::vector<uint64_t> patchExpansions;
			const TechniqueEntry* psTechEntry = &_noPatches;
			switch (CalculateIllumType(*shaderPatches)) {
			case IllumType::PerPixel:
				psTechEntry = &_perPixel;
				patchExpansions.insert(patchExpansions.end(), s_patchExp_perPixel, &s_patchExp_perPixel[dimof(s_patchExp_perPixel)]);
				break;
			case IllumType::PerPixelAndEarlyRejection:
				psTechEntry = &_perPixelAndEarlyRejection;
				patchExpansions.insert(patchExpansions.end(), s_patchExp_perPixelAndEarlyRejection, &s_patchExp_perPixelAndEarlyRejection[dimof(s_patchExp_perPixelAndEarlyRejection)]);
				break;
			default:
				break;
			}

			const TechniqueEntry* vsTechEntry = &_vsNoPatchesSrc;
			if (shaderPatches->GetInterface().HasPatchType(s_deformVertex)) {
				vsTechEntry = &_vsDeformVertexSrc;
				patchExpansions.push_back(s_deformVertex);
			}

			TechniqueEntry mergedTechEntry = *vsTechEntry;
			mergedTechEntry.MergeIn(*psTechEntry);

			GraphicsPipelineDesc result;
			result._shaderProgram = ResolveVariation(shaderPatches, selectors, mergedTechEntry, MakeIteratorRange(patchExpansions));
			result._rasterization = BuildDefaultRastizerDesc(stateSet);

			if (stateSet._flag & Assets::RenderStateSet::Flag::ForwardBlend) {
                result._blend = AttachmentBlendDesc {
					stateSet._forwardBlendOp != BlendOp::NoBlending,
					stateSet._forwardBlendSrc, stateSet._forwardBlendDst, stateSet._forwardBlendOp };
            } else {
                result._blend = Techniques::CommonResources()._abOpaque;
            }
			result._depthStencil = _depthStencil;
			return result;
		}

		TechniqueDelegate_Forward(
			const std::shared_ptr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources,
			TechniqueDelegateForwardFlags::BitField flags)
		: _techniqueSet(techniqueSet)
		{
			_sharedResources = sharedResources;

			_depthStencil = {};
			if (flags & TechniqueDelegateForwardFlags::DisableDepthWrite) {
				_depthStencil = Techniques::CommonResources()._dsReadOnly;
			}

			const auto noPatchesHash = Hash64("Forward_NoPatches");
			const auto perPixelHash = Hash64("Forward_PerPixel");
			const auto perPixelAndEarlyRejectionHash = Hash64("Forward_PerPixelAndEarlyRejection");
			const auto vsNoPatchesHash = Hash64("VS_NoPatches");
			const auto vsDeformVertexHash = Hash64("VS_DeformVertex");
			auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
			auto* perPixelSrc = _techniqueSet->FindEntry(perPixelHash);
			auto* perPixelAndEarlyRejectionSrc = _techniqueSet->FindEntry(perPixelAndEarlyRejectionHash);
			auto* vsNoPatchesSrc = _techniqueSet->FindEntry(vsNoPatchesHash);
			auto* vsDeformVertexSrc = _techniqueSet->FindEntry(vsDeformVertexHash);
			if (!noPatchesSrc || !perPixelSrc || !perPixelAndEarlyRejectionSrc || !vsNoPatchesSrc || !vsDeformVertexSrc) {
				Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
			}

			_noPatches = *noPatchesSrc;
			_perPixel = *perPixelSrc;
			_perPixelAndEarlyRejection = *perPixelAndEarlyRejectionSrc;
			_vsNoPatchesSrc = *vsNoPatchesSrc;
			_vsDeformVertexSrc = *vsDeformVertexSrc;
		}
	private:
		std::shared_ptr<TechniqueSetFile> _techniqueSet;
		TechniqueEntry _noPatches;
		TechniqueEntry _perPixel;
		TechniqueEntry _perPixelAndEarlyRejection;
		DepthStencilDesc _depthStencil;
		TechniqueEntry _vsNoPatchesSrc;
		TechniqueEntry _vsDeformVertexSrc;
	};

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_Forward(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources,
		TechniqueDelegateForwardFlags::BitField flags)
	{
		return std::make_shared<TechniqueDelegate_Forward>(techniqueSet, sharedResources, flags);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_DepthOnly : public TechniqueDelegate_Base
	{
	public:
		GraphicsPipelineDesc Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			std::vector<uint64_t> patchExpansions;
			const TechniqueEntry* psTechEntry = &_noPatches;
			if (shaderPatches->GetInterface().HasPatchType(s_earlyRejectionTest)) {
				psTechEntry = &_earlyRejectionSrc;
				patchExpansions.insert(patchExpansions.end(), s_patchExp_earlyRejection, &s_patchExp_perPixel[dimof(s_patchExp_earlyRejection)]);
			}

			const TechniqueEntry* vsTechEntry = &_vsNoPatchesSrc;
			if (shaderPatches->GetInterface().HasPatchType(s_deformVertex)) {
				vsTechEntry = &_vsDeformVertexSrc;
				patchExpansions.push_back(s_deformVertex);
			}

			TechniqueEntry mergedTechEntry = *vsTechEntry;
			mergedTechEntry.MergeIn(*psTechEntry);

			GraphicsPipelineDesc result;
			result._shaderProgram = ResolveVariation(shaderPatches, selectors, mergedTechEntry, MakeIteratorRange(patchExpansions));

			unsigned cullDisable = 0;
			if (stateSet._flag & Assets::RenderStateSet::Flag::DoubleSided)
				cullDisable = !!stateSet._doubleSided;
			result._rasterization = _rs[cullDisable];
			return result;
		}

		TechniqueDelegate_DepthOnly(
			const std::shared_ptr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources,
			const RSDepthBias& singleSidedBias,
			const RSDepthBias& doubleSidedBias,
			CullMode cullMode,
			bool shadowGen)
		: _techniqueSet(techniqueSet)
		{
			_sharedResources = sharedResources;

			_rs[0x0] = RasterizationDesc{cullMode,        FaceWinding::CCW, (float)singleSidedBias._depthBias, singleSidedBias._depthBiasClamp, singleSidedBias._slopeScaledBias};
            _rs[0x1] = RasterizationDesc{CullMode::None,  FaceWinding::CCW, (float)doubleSidedBias._depthBias, doubleSidedBias._depthBiasClamp, doubleSidedBias._slopeScaledBias};

			const auto noPatchesHash = Hash64("DepthOnly_NoPatches");
			const auto earlyRejectionHash = Hash64("DepthOnly_EarlyRejection");
			auto vsNoPatchesHash = Hash64("VS_NoPatches");
			auto vsDeformVertexHash = Hash64("VS_DeformVertex");
			if (shadowGen) {
				vsNoPatchesHash = Hash64("VSShadowGen_NoPatches");
				vsDeformVertexHash = Hash64("VSShadowGen_DeformVertex");
			}
			auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
			auto* earlyRejectionSrc = _techniqueSet->FindEntry(earlyRejectionHash);
			auto* vsNoPatchesSrc = _techniqueSet->FindEntry(vsNoPatchesHash);
			auto* vsDeformVertexSrc = _techniqueSet->FindEntry(vsDeformVertexHash);
			if (!noPatchesSrc || !earlyRejectionSrc || !vsNoPatchesSrc || !vsDeformVertexSrc) {
				Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
			}

			_noPatches = *noPatchesSrc;
			_earlyRejectionSrc = *earlyRejectionSrc;
			_vsNoPatchesSrc = *vsNoPatchesSrc;
			_vsDeformVertexSrc = *vsDeformVertexSrc;
		}
	private:
		std::shared_ptr<TechniqueSetFile> _techniqueSet;
		TechniqueEntry _noPatches;
		TechniqueEntry _earlyRejectionSrc;
		TechniqueEntry _vsNoPatchesSrc;
		TechniqueEntry _vsDeformVertexSrc;

		RasterizationDesc _rs[2];
	};

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_DepthOnly(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources,
		const RSDepthBias& singleSidedBias,
        const RSDepthBias& doubleSidedBias,
        CullMode cullMode)
	{
		return std::make_shared<TechniqueDelegate_DepthOnly>(techniqueSet, sharedResources, singleSidedBias, doubleSidedBias, cullMode, false);
	}

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_ShadowGen(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources,
		const RSDepthBias& singleSidedBias,
        const RSDepthBias& doubleSidedBias,
        CullMode cullMode)
	{
		return std::make_shared<TechniqueDelegate_DepthOnly>(techniqueSet, sharedResources, singleSidedBias, doubleSidedBias, cullMode, true);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_RayTest : public TechniqueDelegate_Base
	{
	public:
		GraphicsPipelineDesc Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			std::vector<uint64_t> patchExpansions;
			const TechniqueEntry* psTechEntry = &_noPatches;
			if (shaderPatches->GetInterface().HasPatchType(s_earlyRejectionTest)) {
				psTechEntry = &_earlyRejectionSrc;
				patchExpansions.insert(patchExpansions.end(), s_patchExp_earlyRejection, &s_patchExp_perPixel[dimof(s_patchExp_earlyRejection)]);
			}

			const TechniqueEntry* vsTechEntry = &_vsNoPatchesSrc;
			if (shaderPatches->GetInterface().HasPatchType(s_deformVertex)) {
				vsTechEntry = &_vsDeformVertexSrc;
				patchExpansions.push_back(s_deformVertex);
			}

			TechniqueEntry mergedTechEntry = *vsTechEntry;
			mergedTechEntry.MergeIn(*psTechEntry);

			GraphicsPipelineDesc result;
			result._shaderProgram = ResolveVariation(shaderPatches, selectors, mergedTechEntry, MakeIteratorRange(patchExpansions));
			result._depthStencil = CommonResources()._dsDisable;
			// result._rasterization = CommonResources()._rsDisable;
			return result;
		}

		TechniqueDelegate_RayTest(
			const std::shared_ptr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources,
			const StreamOutputInitializers& soInit)
		: _techniqueSet(techniqueSet)
		{
			_sharedResources = sharedResources;
			_soElements = std::vector<InputElementDesc>(soInit._outputElements.begin(), soInit._outputElements.end());
			_soStrides = std::vector<unsigned>(soInit._outputBufferStrides.begin(), soInit._outputBufferStrides.end());

			const auto noPatchesHash = Hash64("RayTest_NoPatches");
			const auto earlyRejectionHash = Hash64("RayTest_EarlyRejection");
			const auto vsNoPatchesHash = Hash64("VS_NoPatches");
			const auto vsDeformVertexHash = Hash64("VS_DeformVertex");
			auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
			auto* earlyRejectionSrc = _techniqueSet->FindEntry(earlyRejectionHash);
			auto* vsNoPatchesSrc = _techniqueSet->FindEntry(vsNoPatchesHash);
			auto* vsDeformVertexSrc = _techniqueSet->FindEntry(vsDeformVertexHash);
			if (!noPatchesSrc || !earlyRejectionSrc || !vsNoPatchesSrc || !vsDeformVertexSrc) {
				Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
			}

			_noPatches = *noPatchesSrc;
			_earlyRejectionSrc = *earlyRejectionSrc;
			_vsNoPatchesSrc = *vsNoPatchesSrc;
			_vsDeformVertexSrc = *vsDeformVertexSrc;
		}
	private:
		std::shared_ptr<TechniqueSetFile> _techniqueSet;
		TechniqueEntry _noPatches;
		TechniqueEntry _earlyRejectionSrc;
		TechniqueEntry _vsNoPatchesSrc;
		TechniqueEntry _vsDeformVertexSrc;
	};

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_RayTest(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources,
		const StreamOutputInitializers& soInit)
	{
		return std::make_shared<TechniqueDelegate_RayTest>(techniqueSet, sharedResources, soInit);
	}
#endif

	ITechniqueDelegate::~ITechniqueDelegate() {}

}}

