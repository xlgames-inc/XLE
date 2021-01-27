// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueDelegates.h"
#include "CompiledShaderPatchCollection.h"
#include "CommonResources.h"
#include "CommonUtils.h"
#include "../Assets/RawMaterial.h"
#include "../Assets/LocalCompiledShaderSource.h"
#include "../Assets/Services.h"
#include "../Metal/Shader.h"
#include "../Metal/ObjectFactory.h"
#include "../IDevice.h"
#include "../Format.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/IntermediatesStore.h"			// for GetDependentFileState()
#include "../../Assets/AssetFutureContinuation.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/StringFormat.h"
#include "../../xleres/FileList.h"
#include <sstream>
#include <regex>
#include <cctype>
#include <charconv>

namespace RenderCore { namespace Techniques
{
	static const uint64_t CompileProcess_InstantiateShaderGraph = ConstHash64<'Inst', 'shdr'>::Value;

	auto AssembleShader(
		const CompiledShaderPatchCollection& patchCollection,
		IteratorRange<const uint64_t*> redirectedPatchFunctions,
		StringSection<> definesTable) -> RenderCore::Assets::ISourceCodePreprocessor::SourceCodeWithRemapping
	{
		// We can assemble the final shader in 3 fragments:
		//  1) the source code in CompiledShaderPatchCollection
		//  2) redirection functions (which redirect from the template function names to the concrete instantiations we want to tie in)
		//  3) include the entry point function itself

		std::stringstream output;

		// Extremely awkwardly; we must go from the "definesTable" format back into a ParameterBox
		// The defines table itself was probably built from a ParameterBox. But we can't pass complex
		// types through the asset compiler interface, so we always end up having to pass them in some
		// kind of string form
		ParameterBox paramBoxSelectors;
		auto p = definesTable.begin();
        while (p != definesTable.end()) {
            while (p != definesTable.end() && std::isspace(*p)) ++p;

            auto definition = std::find(p, definesTable.end(), '=');
            auto defineEnd = std::find(p, definesTable.end(), ';');

            auto endOfName = std::min(defineEnd, definition);
            while ((endOfName-1) > p && std::isspace(*(endOfName-1))) ++endOfName;

            if (definition < defineEnd) {
                auto e = definition+1;
                while (e < defineEnd && std::isspace(*e)) ++e;
				paramBoxSelectors.SetParameter(MakeStringSection(p, endOfName).Cast<utf8>(), MakeStringSection(e, defineEnd));
            } else {
				paramBoxSelectors.SetParameter(MakeStringSection(p, endOfName).Cast<utf8>(), {}, ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Void});
            }

            p = (defineEnd == definesTable.end()) ? defineEnd : (defineEnd+1);
        }
		output << patchCollection.InstantiateShader(paramBoxSelectors);

		for (auto fn:redirectedPatchFunctions) {
			auto i = std::find_if(
				patchCollection.GetInterface().GetPatches().begin(), patchCollection.GetInterface().GetPatches().end(),
				[fn](const CompiledShaderPatchCollection::Interface::Patch& p) { return p._implementsHash == fn; });
			assert(i!=patchCollection.GetInterface().GetPatches().end());
			if (i == patchCollection.GetInterface().GetPatches().end()) {
				Log(Warning) << "Could not find matching patch function for hash (" << fn << ")" << std::endl;
				continue;
			}

			// GenerateScaffoldFunction just creates a function with the name of the template
			// that calls the specific implementation requested.
			// This is important, because the entry point shader code will call the function
			// using that template function name. The raw input source code won't have any implementation
			// for that -- just the function signature.
			// So we provide the implementation here, in the form of a scaffold function
			if (!i->_scaffoldInFunction.empty())
				output << i->_scaffoldInFunction;
		}

		RenderCore::Assets::ISourceCodePreprocessor::SourceCodeWithRemapping result;
		result._processedSource = output.str();
		/*result._dependencies.insert(
			result._dependencies.end(),
			patchCollection._dependencies.begin(), patchCollection._dependencies.end());*/

		// We could fill in the _lineMarkers member with some line marker information
		// from the original shader graph compile; but that might be overkill

		return result;
	}

	class InstantiateShaderGraphPreprocessor : public RenderCore::Assets::ISourceCodePreprocessor
	{
	public:
		SourceCodeWithRemapping AssembleDirectFromFile(StringSection<> filename)
		{
			assert(!XlEqString(filename, "-0"));

			// Fall back to loading the file directly (without any real preprocessing)
			SourceCodeWithRemapping result;
			result._dependencies.push_back(::Assets::IntermediatesStore::GetDependentFileState(filename));

			size_t sizeResult = 0;
			auto blob = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(filename, &sizeResult);
			result._processedSource = std::string((char*)blob.get(), (char*)PtrAdd(blob.get(), sizeResult));
			result._lineMarkers.push_back(RenderCore::ILowLevelCompiler::SourceLineMarker{filename.AsString(), 0, 0});
			return result;
		}

		virtual SourceCodeWithRemapping RunPreprocessor(StringSection<> filename, StringSection<> definesTable) override
		{
			// Encoded in the filename is the guid for the CompiledShaderPatchCollection, the list of functions that require
			// redirection and the entry point shader filename
			
			static std::regex filenameExp(R"--(([^-]+)-([0-9,a-f,A-F]{1,16})(?:-([0-9,a-f,A-F]{1,16}))*)--");
			std::cmatch matches;
			if (!std::regex_match(filename.begin(), filename.end(), matches, filenameExp) || matches.size() < 3)
				return AssembleDirectFromFile(filename);		// don't understand the input filename, we can't expand this

			uint64_t patchCollectionGuid = 0;
			auto fromCharsResult = std::from_chars(matches[2].first, matches[2].second, patchCollectionGuid, 16);
			if (fromCharsResult.ec != std::errc{} || fromCharsResult.ptr != matches[2].second)
				return AssembleDirectFromFile(filename);

			auto i = LowerBound(_registry, patchCollectionGuid);
			if (i == _registry.end() || i->first != patchCollectionGuid)
				return AssembleDirectFromFile(MakeStringSection(matches[1].first, matches[1].second));		// don't understand the input filename, we can't expand this

			auto& patchCollection = *i->second;
			if (patchCollection.GetInterface().GetPatches().empty())
				return AssembleDirectFromFile(MakeStringSection(matches[1].first, matches[1].second));

			std::vector<uint64_t> redirectedPatchFunctions;
			redirectedPatchFunctions.reserve(matches.size() - 3);
			for (auto m=matches.begin()+3; m!=matches.end(); ++m)
				if (m->matched) {
					uint64_t guid = 0;
					fromCharsResult = std::from_chars(m->first, m->second, guid, 16);
					if (fromCharsResult.ec != std::errc{} || fromCharsResult.ptr != matches[2].second)
						Throw(std::runtime_error("Integer parsing failure"));
					redirectedPatchFunctions.push_back(guid);
				}

			auto result = AssembleShader(patchCollection, MakeIteratorRange(redirectedPatchFunctions), definesTable);

			// For simplicity, we'll just append the entry point file using an #include directive
			// This will ensure we go through the normal mechanisms to find and load this file.
			// Note that this relies on the underlying shader compiler supporting #includes, however
			//   -- in cases  (like GLSL) that don't have #include support, we would need another
			//	changed preprocessor to handle the include expansions.
			{
				std::stringstream str;
				str << "#include \"" << MakeStringSection(matches[1].first, matches[1].second) << "\"" << std::endl;
				result._processedSource += str.str();
			}

			return result;
		}

		InstantiateShaderGraphPreprocessor() {}
		~InstantiateShaderGraphPreprocessor() {}

		void Register(uint64_t id, const std::shared_ptr<CompiledShaderPatchCollection>& patchCollection)
		{
			if (id == 0)
				return;

			auto i = LowerBound(_registry, id);
			if (i != _registry.end() && i->first == id) {
				// assert(i->second == patchCollection);
			} else {
				_registry.insert(i, std::make_pair(id, patchCollection));
			}
		}

	private:
		std::vector<std::pair<uint64_t, std::shared_ptr<CompiledShaderPatchCollection>>> _registry;
	};

#if 0		// todo -- need to bring this back
	static const std::shared_ptr<InstantiateShaderGraphPreprocessor>& GetInstantiateShaderGraphPreprocessor()
	{
		static std::shared_ptr<InstantiateShaderGraphPreprocessor> singleton;
		if (!singleton) {
			singleton = std::make_shared<InstantiateShaderGraphPreprocessor>();
			auto shaderSource = std::make_shared<RenderCore::Assets::LocalCompiledShaderSource>(
				RenderCore::Assets::Services::GetDevice().CreateShaderCompiler(),
				singleton,
				RenderCore::Assets::Services::GetDevice().GetDesc(),
				CompileProcess_InstantiateShaderGraph);
			RenderCore::ShaderService::GetInstance().AddShaderSource(shaderSource);
			::Assets::Services::GetAsyncMan().GetIntermediateCompilers().AddCompiler(shaderSource);
		}
		return singleton;
	}
#endif

	class CompiledShaderByteCode_InstantiateShaderGraph : public RenderCore::CompiledShaderByteCode
	{
	public:
		static const uint64 CompileProcessType = CompileProcess_InstantiateShaderGraph;

		using CompiledShaderByteCode::CompiledShaderByteCode;
	};

	class ShaderPatchFactory : public IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<CompiledShaderByteCode> MakeByteCodeFuture(
			ShaderStage stage, StringSection<> initializer, StringSection<> definesTable)
		{
			assert(!initializer.IsEmpty());

			char temp[MaxPath];
			auto meld = StringMeldInPlace(temp);
			auto sep = std::find(initializer.begin(), initializer.end(), ':');
			meld << MakeStringSection(initializer.begin(), sep);

			// patch collection & expansions
			if (!XlEqString(MakeStringSection(initializer.begin(), sep), "null")
				&& _patchCollection && _patchCollection->GetGUID() != 0) {
				meld << "-" << std::hex << _patchCollection->GetGUID();
				for (auto exp:_patchExpansions) meld << "-" << exp;
			}

			meld << MakeStringSection(sep, initializer.end());

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
				}
				if (!XlFindStringI(initializer, profileStr)) {
					meld << ":" << profileStr << "*";
				}
			}

			auto ret = ::Assets::MakeAsset<CompiledShaderByteCode_InstantiateShaderGraph>(MakeStringSection(temp), definesTable);
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

				return RenderCore::Techniques::CreateShaderProgramFromByteCode(
					vsCode, gsCode, psCode,
					StreamOutputInitializers {
						MakeIteratorRange(_soElements), MakeIteratorRange(_soStrides)
					},
					"ShaderPatchFactory");
			} else {
				return RenderCore::Techniques::CreateShaderProgramFromByteCode(vsCode, psCode, "ShaderPatchFactory");
			}
		}

		ShaderPatchFactory(
			const TechniqueEntry& techEntry, 
			const std::shared_ptr<CompiledShaderPatchCollection>& patchCollection,
			IteratorRange<const uint64_t*> patchExpansions,
			const StreamOutputInitializers& so = {})
		: _entry(&techEntry)
		, _patchCollection(patchCollection)
		, _patchExpansions(patchExpansions)
		{
			if (_patchCollection) {
				assert(0);	// todo -- temporarily broken
				/*GetInstantiateShaderGraphPreprocessor()->Register(
					_patchCollection->GetGUID(),
					_patchCollection);*/
			}

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
				_soElements = std::vector<RenderCore::InputElementDesc>(so._outputElements.begin(), so._outputElements.end());
				_soStrides = std::vector<unsigned>(so._outputBufferStrides.begin(), so._outputBufferStrides.end());

				_factoryGuid = HashCombine(Hash64(_soExtraDefines), _factoryGuid);
				_factoryGuid = HashCombine(Hash64(so._outputBufferStrides.begin(), so._outputBufferStrides.end()), _factoryGuid);
			}
		}
		ShaderPatchFactory() {}
	private:
		const TechniqueEntry* _entry;
		std::shared_ptr<CompiledShaderPatchCollection> _patchCollection;
		IteratorRange<const uint64_t*> _patchExpansions;

		std::string _soExtraDefines;
		std::vector<RenderCore::InputElementDesc> _soElements;
		std::vector<unsigned> _soStrides;
	};

////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_Legacy : public ITechniqueDelegate
	{
	public:
		ResolvedTechnique Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& input) override;

		TechniqueDelegate_Legacy(
			unsigned techniqueIndex,
			const RenderCore::AttachmentBlendDesc& blend,
			const RenderCore::RasterizationDesc& rasterization,
			const RenderCore::DepthStencilDesc& depthStencil);
		~TechniqueDelegate_Legacy();
	private:
		unsigned _techniqueIndex;
		RenderCore::AttachmentBlendDesc _blend;
		RenderCore::RasterizationDesc _rasterization;
		RenderCore::DepthStencilDesc _depthStencil;

		struct VariationSet
		{
			std::shared_ptr<Technique> _techniqueSetFuture;
			std::shared_ptr<TechniqueShaderVariationSet> _variationSet;
			const ::Assets::DepValPtr& GetDependencyValidation() const { return _techniqueSetFuture->GetDependencyValidation(); }
		};
		::Assets::FuturePtr<VariationSet> _variationSetFuture;
	};

	auto TechniqueDelegate_Legacy::Resolve(
		const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
		IteratorRange<const ParameterBox**> selectors,
		const RenderCore::Assets::RenderStateSet& input) -> ResolvedTechnique
	{
		ResolvedTechnique result;
		result._blend = _blend;
		result._rasterization = _rasterization;
		result._depthStencil = _depthStencil;

		auto variationSet = _variationSetFuture->TryActualize();
		if (variationSet) {
			result._shaderProgram = variationSet->_variationSet->FindVariation(_techniqueIndex, selectors.begin());
		} else {
			std::vector<ParameterBox> selectorsCopy;
			for (const auto&sel:selectors) selectorsCopy.push_back(*sel);
			result._shaderProgram = std::make_shared<::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>>("ShaderPendingVariationSet");
			::Assets::WhenAll(_variationSetFuture).ThenConstructToFuture<RenderCore::Metal::ShaderProgram>(
				*result._shaderProgram,
				[techniqueIndex{_techniqueIndex}, selectorsCopy](const std::shared_ptr<VariationSet>& variationSet) {
					const ParameterBox* selectorPtrs[ShaderSelectorFiltering::Source::Max] = {};
					for (size_t c=0; c<std::min(selectorsCopy.size(), dimof(selectorPtrs)); ++c)
						selectorPtrs[c] = &selectorsCopy[c];
					auto shader = variationSet->_variationSet->FindVariation(techniqueIndex, selectorPtrs);
					shader->StallWhilePending();
					return shader->Actualize();
				});
		}

		return result;
	}

	TechniqueDelegate_Legacy::TechniqueDelegate_Legacy(
		unsigned techniqueIndex,
		const RenderCore::AttachmentBlendDesc& blend,
		const RenderCore::RasterizationDesc& rasterization,
		const RenderCore::DepthStencilDesc& depthStencil)
	: _techniqueIndex(techniqueIndex)
	, _blend(blend)
	, _rasterization(rasterization)
	, _depthStencil(depthStencil)
	{
		const char* techFile = ILLUM_LEGACY_TECH;
		auto techniqueSetFuture = ::Assets::MakeAsset<Technique>(techFile);
		_variationSetFuture = std::make_shared<::Assets::AssetFuture<VariationSet>>(techFile);
		::Assets::WhenAll(techniqueSetFuture).ThenConstructToFuture<VariationSet>(
			*_variationSetFuture,
			[](const std::shared_ptr<Technique>& technique) {
				auto variationSet = std::make_shared<TechniqueShaderVariationSet>(technique);
				return std::make_shared<VariationSet>(VariationSet{
					technique,
					variationSet
					});
			});
	}

	TechniqueDelegate_Legacy::~TechniqueDelegate_Legacy()
	{
	}

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegateLegacy(
		unsigned techniqueIndex,
		const RenderCore::AttachmentBlendDesc& blend,
		const RenderCore::RasterizationDesc& rasterization,
		const RenderCore::DepthStencilDesc& depthStencil)
	{
		return std::make_shared<TechniqueDelegate_Legacy>(techniqueIndex, blend, rasterization, depthStencil);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		T E C H N I Q U E   D E L E G A T E
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_Base : public ITechniqueDelegate
	{
	protected:
		::Assets::FuturePtr<RenderCore::Metal::ShaderProgram> ResolveVariation(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const TechniqueEntry& techEntry,
			IteratorRange<const uint64_t*> patchExpansions)
		{
			StreamOutputInitializers soInit;
			soInit._outputElements = MakeIteratorRange(_soElements);
			soInit._outputBufferStrides = MakeIteratorRange(_soStrides);

			ShaderPatchFactory factory(techEntry, shaderPatches, patchExpansions, soInit);
			const auto& variation = _sharedResources->_mainVariationSet.FindVariation(
				selectors,
				techEntry._selectorFiltering,
				shaderPatches->GetInterface().GetSelectorRelevance(),
				factory);
			return variation._shaderFuture;
		}

		std::shared_ptr<TechniqueSharedResources> _sharedResources;
		std::vector<RenderCore::InputElementDesc> _soElements;
		std::vector<unsigned> _soStrides;
	};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static const auto s_perPixel = Hash64("PerPixel");
	static const auto s_earlyRejectionTest = Hash64("EarlyRejectionTest");
	static const auto s_deformVertex = Hash64("DeformVertex");
	static uint64_t s_patchExp_perPixelAndEarlyRejection[] = { s_perPixel, s_earlyRejectionTest };
	static uint64_t s_patchExp_perPixel[] = { s_perPixel };
	static uint64_t s_patchExp_earlyRejection[] = { s_earlyRejectionTest };

	IllumType CalculateIllumType(const CompiledShaderPatchCollection& patchCollection)
	{
		if (patchCollection.GetInterface().HasPatchType(s_perPixel)) {
			if (patchCollection.GetInterface().HasPatchType(s_earlyRejectionTest)) {
				return IllumType::PerPixelAndEarlyRejection;
			} else {
				return IllumType::PerPixel;
			}
		}
		return IllumType::NoPerPixel;
	}

	class TechniqueDelegate_Deferred : public TechniqueDelegate_Base
	{
	public:
		ResolvedTechnique Resolve(
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

			// note -- we could premerge all of the combinations in the constructor, to cut down on cost here
			TechniqueEntry mergedTechEntry = *vsTechEntry;
			mergedTechEntry.MergeIn(*psTechEntry);

			ResolvedTechnique result;
			result._shaderProgram = ResolveVariation(shaderPatches, selectors, mergedTechEntry, MakeIteratorRange(patchExpansions));
			result._rasterization = BuildDefaultRastizerDesc(stateSet);

			bool deferredDecal = 
					(stateSet._flag & Assets::RenderStateSet::Flag::BlendType)
				&&	(stateSet._blendType == Assets::RenderStateSet::BlendType::DeferredDecal);
			result._blend = deferredDecal
				? CommonResources()._abStraightAlpha
				: CommonResources()._abOpaque;
			return result;
		}

		TechniqueDelegate_Deferred(
			const std::shared_ptr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources)
		: _techniqueSet(techniqueSet)
		{
			_sharedResources = sharedResources;

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
	private:
		std::shared_ptr<TechniqueSetFile> _techniqueSet;
		TechniqueEntry _noPatches;
		TechniqueEntry _perPixel;
		TechniqueEntry _perPixelAndEarlyRejection;
		TechniqueEntry _vsNoPatchesSrc;
		TechniqueEntry _vsDeformVertexSrc;
	};

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate_Deferred(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	{
		return std::make_shared<TechniqueDelegate_Deferred>(techniqueSet, sharedResources);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class TechniqueDelegate_Forward : public TechniqueDelegate_Base
	{
	public:
		ResolvedTechnique Resolve(
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

			ResolvedTechnique result;
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
		ResolvedTechnique Resolve(
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

			ResolvedTechnique result;
			result._shaderProgram = ResolveVariation(shaderPatches, selectors, mergedTechEntry, MakeIteratorRange(patchExpansions));

			unsigned cullDisable    = !!(stateSet._flag & Assets::RenderStateSet::Flag::DoubleSided);
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
		ResolvedTechnique Resolve(
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

			ResolvedTechnique result;
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
			_soElements = std::vector<RenderCore::InputElementDesc>(soInit._outputElements.begin(), soInit._outputElements.end());
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

	ITechniqueDelegate::~ITechniqueDelegate() {}

}}

