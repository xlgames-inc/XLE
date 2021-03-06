// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueDelegates.h"
#include "Drawables.h"
#include "CompiledShaderPatchCollection.h"
#include "CommonResources.h"
#include "CommonUtils.h"
#include "AutomaticSelectorFiltering.h"
#include "../Assets/RawMaterial.h"
#include "../Assets/LocalCompiledShaderSource.h"
#include "../Assets/Services.h"
// #include "../Metal/Shader.h"
// #include "../Metal/ObjectFactory.h"
#include "../IDevice.h"
#include "../Format.h"
// #include "../MinimalShaderSource.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
// #include "../../Assets/AssetServices.h"
#include "../../Assets/IntermediatesStore.h"			// for GetDependentFileState()
#include "../../Assets/AssetFutureContinuation.h"
#include "../../ConsoleRig/GlobalServices.h"			// for GetLibVersionDesc
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
	const CommonResourceBox& CommonResources()
	{
		assert(0);
		return *(const CommonResourceBox*)nullptr;
	}
	
	static const uint64_t CompileProcess_InstantiateShaderGraph = ConstHash64<'Inst', 'shdr'>::Value;

	auto AssembleShader(
		const CompiledShaderPatchCollection& patchCollection,
		IteratorRange<const uint64_t*> redirectedPatchFunctions,
		StringSection<> definesTable) -> ISourceCodePreprocessor::SourceCodeWithRemapping

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

		ISourceCodePreprocessor::SourceCodeWithRemapping result;
		result._processedSource = output.str();
		result._dependencies.insert(
			result._dependencies.end(),
			patchCollection._dependencies.begin(), patchCollection._dependencies.end());

		// We could fill in the _lineMarkers member with some line marker information
		// from the original shader graph compile; but that might be overkill
		return result;
	}

	static auto AssembleDirectFromFile(StringSection<> filename) -> ISourceCodePreprocessor::SourceCodeWithRemapping
	{
		assert(!XlEqString(filename, "-0"));

		// Fall back to loading the file directly (without any real preprocessing)
		ISourceCodePreprocessor::SourceCodeWithRemapping result;
		result._dependencies.push_back(::Assets::IntermediatesStore::GetDependentFileState(filename));

		size_t sizeResult = 0;
		auto blob = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(filename, &sizeResult);
		result._processedSource = std::string((char*)blob.get(), (char*)PtrAdd(blob.get(), sizeResult));
		result._lineMarkers.push_back(ILowLevelCompiler::SourceLineMarker{filename.AsString(), 0, 0});
		return result;
	}

	static auto InstantiateShaderGraph_CompileFromFile(
		ShaderService::IShaderSource& internalShaderSource,
		const ILowLevelCompiler::ResId& resId, 
		StringSection<> definesTable,
		const CompiledShaderPatchCollection& patchCollection,
		IteratorRange<const uint64_t*> redirectedPatchFunctions) -> ShaderService::IShaderSource::ShaderByteCodeBlob
	{
		if (patchCollection.GetInterface().GetPatches().empty())
			return internalShaderSource.CompileFromFile(resId, definesTable);

		auto assembledShader = AssembleShader(patchCollection, redirectedPatchFunctions, definesTable);

		// For simplicity, we'll just append the entry point file using an #include directive
		// This will ensure we go through the normal mechanisms to find and load this file.
		// Note that this relies on the underlying shader compiler supporting #includes, however
		//   -- in cases  (like GLSL) that don't have #include support, we would need another
		//	changed preprocessor to handle the include expansions.
		{
			std::stringstream str ;
			str << "#include \"" << resId._filename << "\"" << std::endl;
			assembledShader._processedSource += str.str();
		}

		auto result = internalShaderSource.CompileFromMemory(
			MakeStringSection(assembledShader._processedSource),
			resId._entryPoint, resId._shaderModel,
			definesTable);

		result._deps.insert(result._deps.end(), assembledShader._dependencies.begin(), assembledShader._dependencies.end());
		return result;
	}

	class CompiledShaderByteCode_InstantiateShaderGraph : public CompiledShaderByteCode
	{
	public:
		static const uint64 CompileProcessType = CompileProcess_InstantiateShaderGraph;

		using CompiledShaderByteCode::CompiledShaderByteCode;
	};

	static const auto ChunkType_Log = ConstHash64<'Log'>::Value;
	class ShaderGraphCompileOperation : public ::Assets::ICompileOperation
	{
	public:
		virtual std::vector<TargetDesc> GetTargets() const override
		{
			return {
				TargetDesc { CompileProcess_InstantiateShaderGraph, "main" }
			};
		}
		
		virtual std::vector<SerializedArtifact> SerializeTarget(unsigned idx) override
		{
			std::vector<SerializedArtifact> result;
			if (_byteCode._payload)
				result.push_back({
					CompileProcess_InstantiateShaderGraph, 0, "main",
					_byteCode._payload});
			if (_byteCode._errors)
				result.push_back({
					ChunkType_Log, 0, "log",
					_byteCode._errors});
			return result;
		}

		virtual std::vector<::Assets::DependentFileState> GetDependencies() const override
		{
			return _byteCode._deps;
		}

		ShaderGraphCompileOperation(
			ShaderService::IShaderSource& shaderSource,
			const ILowLevelCompiler::ResId& resId,
			StringSection<> definesTable,
			const CompiledShaderPatchCollection& patchCollection,
			IteratorRange<const uint64_t*> redirectedPatchFunctions)
		: _byteCode { 
			InstantiateShaderGraph_CompileFromFile(shaderSource, resId, definesTable, patchCollection, redirectedPatchFunctions) 
		}
		{
		}
		
		~ShaderGraphCompileOperation()
		{
		}

		ShaderService::IShaderSource::ShaderByteCodeBlob _byteCode;
	};

	::Assets::IntermediateCompilers::CompilerRegistration RegisterInstantiateShaderGraphCompiler(
		const std::shared_ptr<ShaderService::IShaderSource>& shaderSource,
		::Assets::IntermediateCompilers& intermediateCompilers)

	{
		auto result = intermediateCompilers.RegisterCompiler(
			"shader-graph-compiler",
			ConsoleRig::GetLibVersionDesc(),
			nullptr,
			[shaderSource](const ::Assets::InitializerPack& initializers) {
				return std::make_shared<ShaderGraphCompileOperation>(
					*shaderSource,
					ShaderService::MakeResId(initializers.GetInitializer<std::string>(0)),
					initializers.GetInitializer<std::string>(1),
					*initializers.GetInitializer<std::shared_ptr<CompiledShaderPatchCollection>>(2),
					MakeIteratorRange(initializers.GetInitializer<std::vector<uint64_t>>(3))
				);
			});

		uint64_t outputAssetTypes[] = { CompileProcess_InstantiateShaderGraph };
		intermediateCompilers.AssociateRequest(
			result._registrationId,
			MakeIteratorRange(outputAssetTypes));
		return result;
	}

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
		for (unsigned c=0; c<dimof(nascentDesc->_shaders); ++c)
			nascentDesc->_shaders[c]._manualSelectorFiltering = entry._selectorFiltering;

		auto vsfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer).AllExceptParameters();
		auto psfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer).AllExceptParameters();

		auto vsFilteringFuture = ::Assets::MakeAsset<ShaderSelectorFilteringRules>(vsfn);
		auto psFilteringFuture = ::Assets::MakeAsset<ShaderSelectorFilteringRules>(psfn);
		if (entry._geometryShaderName.empty()) {
			::Assets::WhenAll(vsFilteringFuture, psFilteringFuture).ThenConstructToFuture<ITechniqueDelegate::GraphicsPipelineDesc>(
				future,
				[nascentDesc](
					const std::shared_ptr<ShaderSelectorFilteringRules>& vsFiltering,
					const std::shared_ptr<ShaderSelectorFilteringRules>& psFiltering) {
					
					nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._automaticFiltering = vsFiltering;
					nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._automaticFiltering = psFiltering;
					return nascentDesc;
				});
		} else {
			auto gsfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Geometry]._initializer).AllExceptParameters();
			auto gsFilteringFuture = ::Assets::MakeAsset<ShaderSelectorFilteringRules>(gsfn);
			::Assets::WhenAll(vsFilteringFuture, psFilteringFuture, gsFilteringFuture).ThenConstructToFuture<ITechniqueDelegate::GraphicsPipelineDesc>(
				future,
				[nascentDesc](
					const std::shared_ptr<ShaderSelectorFilteringRules>& vsFiltering,
					const std::shared_ptr<ShaderSelectorFilteringRules>& psFiltering,
					const std::shared_ptr<ShaderSelectorFilteringRules>& gsFiltering) {
					
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
			result->SetPollingFunction(
				[techniqueIndex = _techniqueIndex, techniqueFuture = _techniqueFuture, nascentDesc](::Assets::AssetFuture<GraphicsPipelineDesc>& thatFuture) -> bool {

					::Assets::Blob queriedLog;
					::Assets::DepValPtr queriedDepVal;
					std::shared_ptr<Technique> technique;
					auto state = techniqueFuture->CheckStatusBkgrnd(technique, queriedDepVal, queriedLog);
					if (state == ::Assets::AssetState::Pending)
						return true;

					if (state == ::Assets::AssetState::Invalid) {
						thatFuture.SetInvalidAsset(queriedDepVal, queriedLog);
						return false;
					}

					nascentDesc->_depVal = technique->GetDependencyValidation();
					auto& entry = technique->GetEntry(techniqueIndex);
					PrepareShadersFromTechniqueEntry(thatFuture, nascentDesc, entry);
					return false;
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

	class TechniqueDelegate_Base : public ITechniqueDelegate
	{
	protected:
		::Assets::FuturePtr<Metal::ShaderProgram> ResolveVariation(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const TechniqueEntry& techEntry,
			IteratorRange<const uint64_t*> patchExpansions)
		{
			StreamOutputInitializers soInit;
			soInit._outputElements = MakeIteratorRange(_soElements);
			soInit._outputBufferStrides = MakeIteratorRange(_soStrides);

			ShaderPatchFactory factory(nullptr, techEntry, shaderPatches, patchExpansions, soInit);
			const auto& variation = _sharedResources->_mainVariationSet.FindVariation(
				selectors,
				techEntry._selectorFiltering,
				shaderPatches->GetInterface().GetSelectorRelevance(),
				factory);
			return variation._shaderFuture;
		}

		std::shared_ptr<TechniqueSharedResources> _sharedResources;
		std::vector<InputElementDesc> _soElements;
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

#if 0
	class TechniqueDelegate_Deferred : public TechniqueDelegate_Base
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

			// note -- we could premerge all of the combinations in the constructor, to cut down on cost here
			TechniqueEntry mergedTechEntry = *vsTechEntry;
			mergedTechEntry.MergeIn(*psTechEntry);

			GraphicsPipelineDesc result;
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

	std::shared_ptr<DrawableMaterial> MakeDrawableMaterial(
		const RenderCore::Assets::MaterialScaffoldMaterial& mat,
		const RenderCore::Assets::ShaderPatchCollection& patchCollection)
	{
		auto result = std::make_shared<DrawableMaterial>();
		auto future = ::Assets::MakeAsset<CompiledShaderPatchCollection>(patchCollection);
		future->StallWhilePending();
		result->_patchCollection = future->Actualize();
		result->_material = mat;
		return result;
	}

}}

