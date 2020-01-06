// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueDelegates.h"
#include "DrawableMaterial.h"
#include "CompiledShaderPatchCollection.h"
#include "../Assets/RawMaterial.h"
#include "../Assets/LocalCompiledShaderSource.h"
#include "../Assets/Services.h"
#include "../Metal/Shader.h"
#include "../Metal/ObjectFactory.h"
#include "../IDevice.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/StringFormat.h"
#include <sstream>
#include <regex>
#include <cctype>

namespace RenderCore { namespace Techniques
{
	static const auto s_perPixel = Hash64("PerPixel");
	static const auto s_earlyRejectionTest = Hash64("EarlyRejectionTest");
	
	static bool HasReturn(const GraphLanguage::NodeGraphSignature&);

	static uint64_t CompileProcess_InstantiateShaderGraph = ConstHash64<'Inst', 'shdr'>::Value;

	class InstantiateShaderGraphPreprocessor : public RenderCore::Assets::ISourceCodePreprocessor
	{
	public:
		static SourceCodeWithRemapping AssembleShader(
			const CompiledShaderPatchCollection& patchCollection,
			IteratorRange<const uint64_t*> redirectedPatchFunctions,
			StringSection<> entryPointFileName,
			StringSection<> definesTable)
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
					paramBoxSelectors.SetParameter(MakeStringSection(p, endOfName).Cast<utf8>(), {}, ImpliedTyping::TypeCat::Void);
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

			// For simplicity, we'll just append the entry point file using an #include directive
			// This will ensure we go through the normal mechanisms to find and load this file.
			// Note that this relies on the underlying shader compiler supporting #includes, however
			//   -- in cases  (like GLSL) that don't have #include support, we would need another
			//	changed preprocessor to handle the include expansions.
			output << "#include \"" << entryPointFileName << "\"" << std::endl;

			SourceCodeWithRemapping result;
			result._processedSource = output.str();
			/*result._dependencies.insert(
				result._dependencies.end(),
				patchCollection._dependencies.begin(), patchCollection._dependencies.end());*/

			// We could fill in the _lineMarkers member with some line marker information
			// from the original shader graph compile; but that might be overkill

			return result;
		}

		SourceCodeWithRemapping AssembleDirectFromFile(StringSection<> filename)
		{
			// Fall back to loading the file directly (without any real preprocessing)
			SourceCodeWithRemapping result;
			result._dependencies.push_back(::Assets::IntermediateAssets::Store::GetDependentFileState(filename));

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

			auto patchCollectionGuid = ParseInteger<uint64_t>(MakeStringSection(matches[2].first, matches[2].second), 16).value();
			auto& patchCollection = ShaderPatchCollectionRegistry::GetInstance().GetCompiledShaderPatchCollection(patchCollectionGuid);
			if (!patchCollection || patchCollection->GetInterface().GetPatches().empty())
				return AssembleDirectFromFile(MakeStringSection(matches[1].first, matches[1].second));

			std::vector<uint64_t> redirectedPatchFunctions;
			redirectedPatchFunctions.reserve(matches.size() - 3);
			for (auto m=matches.begin()+3; m!=matches.end(); ++m)
				redirectedPatchFunctions.push_back(ParseInteger<uint64_t>(MakeStringSection(m->first, m->second), 16).value());

			return AssembleShader(*patchCollection, MakeIteratorRange(redirectedPatchFunctions), MakeStringSection(matches[1].first, matches[1].second), definesTable);
		}

		InstantiateShaderGraphPreprocessor() {}
		~InstantiateShaderGraphPreprocessor() {}
	};

	static void TryRegisterDependency(
		::Assets::DepValPtr& dst,
		const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
	{
		auto futureDepVal = future->GetDependencyValidation();
		if (futureDepVal)
			::Assets::RegisterAssetDependency(dst, futureDepVal);
	}

	class ShaderPatchFactory : public IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<CompiledShaderByteCode> MakeByteCodeFuture(
			ShaderStage stage, StringSection<> initializer, StringSection<> definesTable)
		{
			char temp[MaxPath];
			auto meld = StringMeldInPlace(temp);
			auto sep = std::find(initializer.begin(), initializer.end(), ':');
			meld << MakeStringSection(initializer.begin(), sep);

			// patch collection & expansions
			meld << "-" << std::hex << _patchCollection->GetGUID();
			for (auto exp:_patchExpansions) meld << "-" << exp;

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

			StringSection<> initializers[] = { MakeStringSection(temp), definesTable };
			auto future = std::make_shared<::Assets::AssetFuture<CompiledShaderByteCode>>(temp);
			::Assets::DefaultCompilerConstruction<CompiledShaderByteCode>(
				*future,
				initializers, dimof(initializers),
				CompileProcess_InstantiateShaderGraph);
			return future;
		}

		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines)
		{
			::Assets::FuturePtr<CompiledShaderByteCode> vsCode, psCode, gsCode;
			vsCode = MakeByteCodeFuture(ShaderStage::Vertex, _entry->_vertexShaderName, defines);
			psCode = MakeByteCodeFuture(ShaderStage::Pixel, _entry->_pixelShaderName, defines);
			if (!_entry->_geometryShaderName.empty())
				gsCode = MakeByteCodeFuture(ShaderStage::Geometry, _entry->_geometryShaderName, defines);

			auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>("ShaderPatchFactory");
			if (gsCode) {
				future->SetPollingFunction(
					[vsCode, gsCode, psCode](::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>& thatFuture) -> bool {

					auto vsActual = vsCode->TryActualize();
					auto gsActual = gsCode->TryActualize();
					auto psActual = psCode->TryActualize();

					if (!vsActual || !gsActual || !psActual) {
						auto vsState = vsCode->GetAssetState();
						auto gsState = gsCode->GetAssetState();
						auto psState = psCode->GetAssetState();
						if (vsState == ::Assets::AssetState::Invalid || gsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
							auto depVal = std::make_shared<::Assets::DependencyValidation>();
							TryRegisterDependency(depVal, vsCode);
							TryRegisterDependency(depVal, gsCode);
							TryRegisterDependency(depVal, psCode);
							std::stringstream log;
							if (vsState == ::Assets::AssetState::Invalid) log << "Vertex shader is invalid with message: " << std::endl << ::Assets::AsString(vsCode->GetActualizationLog()) << std::endl;
							if (gsState == ::Assets::AssetState::Invalid) log << "Geometry shader is invalid with message: " << std::endl << ::Assets::AsString(gsCode->GetActualizationLog()) << std::endl;
							if (psState == ::Assets::AssetState::Invalid) log << "Pixel shader is invalid with message: " << std::endl << ::Assets::AsString(psCode->GetActualizationLog()) << std::endl;
							thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob(log.str()));
							return false;
						}
						return true;
					}

					auto newShaderProgram = std::make_shared<RenderCore::Metal::ShaderProgram>(
						RenderCore::Metal::GetObjectFactory(), *vsActual, *gsActual, *psActual);
					thatFuture.SetAsset(std::move(newShaderProgram), {});
					return false;
				});
			} else {
				future->SetPollingFunction(
					[vsCode, gsCode, psCode](::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>& thatFuture) -> bool {

					auto vsActual = vsCode->TryActualize();
					auto psActual = psCode->TryActualize();

					if (!vsActual || !psActual) {
						auto vsState = vsCode->GetAssetState();
						auto psState = psCode->GetAssetState();
						if (vsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
							auto depVal = std::make_shared<::Assets::DependencyValidation>();
							TryRegisterDependency(depVal, vsCode);
							TryRegisterDependency(depVal, psCode);
							std::stringstream log;
							if (vsState == ::Assets::AssetState::Invalid) log << "Vertex shader is invalid with message: " << std::endl << ::Assets::AsString(vsCode->GetActualizationLog()) << std::endl;
							if (psState == ::Assets::AssetState::Invalid) log << "Pixel shader is invalid with message: " << std::endl << ::Assets::AsString(psCode->GetActualizationLog()) << std::endl;
							thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob(log.str()));
							return false;
						}
						return true;
					}

					auto newShaderProgram = std::make_shared<RenderCore::Metal::ShaderProgram>(
						RenderCore::Metal::GetObjectFactory(), *vsActual, *psActual);
					thatFuture.SetAsset(std::move(newShaderProgram), {});
					return false;
				});
			}

			return future;
		}

		ShaderPatchFactory(
			const TechniqueEntry& techEntry, 
			const CompiledShaderPatchCollection* patchCollection,
			IteratorRange<const uint64_t*> patchExpansions)
		: _entry(&techEntry)
		, _patchCollection(patchCollection)
		, _patchExpansions(patchExpansions)
		{
			_factoryGuid = _patchCollection ? _patchCollection->GetGUID() : 0;
		}
		ShaderPatchFactory() {}
	private:
		const TechniqueEntry* _entry;
		const CompiledShaderPatchCollection* _patchCollection;
		IteratorRange<const uint64_t*> _patchExpansions;
	};

	static uint64_t s_patchExp_perPixelAndEarlyRejection[] = { s_perPixel, s_earlyRejectionTest };
	static uint64_t s_patchExp_perPixel[] = { s_perPixel };

	RenderCore::Metal::ShaderProgram* TechniqueDelegate_Illum::GetShader(
		ParsingContext& context,
		const ParameterBox* shaderSelectors[],
		const DrawableMaterial& material)
	{
		if (PrimeTechniqueCfg() != ::Assets::AssetState::Ready)
			return nullptr;

		IteratorRange<const uint64_t*> patchExpansions = {};
		const TechniqueEntry* techEntry = &_noPatches;
		using IllumType = CompiledShaderPatchCollection::IllumDelegateAttachment::IllumType;
		switch (material._patchCollection->_illumDelegate._type) {
		case IllumType::PerPixel:
			techEntry = &_perPixel;
			patchExpansions = MakeIteratorRange(s_patchExp_perPixel);
			break;
		case IllumType::PerPixelAndEarlyRejection:
			techEntry = &_perPixelAndEarlyRejection;
			patchExpansions = MakeIteratorRange(s_patchExp_perPixelAndEarlyRejection);
			break;
		default:
			break;
		}

		ShaderPatchFactory factory(*techEntry, material._patchCollection.get(), patchExpansions);
		const auto& variation = _sharedResources->_mainVariationSet.FindVariation(
			techEntry->_baseSelectors, shaderSelectors, factory);
		if (!variation._shaderFuture) return nullptr;
		return variation._shaderFuture->TryActualize().get();
	}

	static void CheckPreprocessInstalled()
	{
		static bool installedPreprocessor = false;
		if (!installedPreprocessor) {
			auto shaderSource = std::make_shared<RenderCore::Assets::LocalCompiledShaderSource>(
				RenderCore::Assets::Services::GetDevice().CreateShaderCompiler(),
				std::make_shared<InstantiateShaderGraphPreprocessor>(),
				RenderCore::Assets::Services::GetDevice().GetDesc(),
				CompileProcess_InstantiateShaderGraph);
			RenderCore::ShaderService::GetInstance().AddShaderSource(shaderSource);
			::Assets::Services::GetAsyncMan().GetIntermediateCompilers().AddCompiler(shaderSource);
			
			installedPreprocessor = true;
		}
	}

	TechniqueDelegate_Illum::TechniqueDelegate_Illum(const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	: _sharedResources(sharedResources)
	{
		_techniqueSetFuture = ::Assets::MakeAsset<TechniqueSetFile>("xleres/Techniques/New/Illum.tech");
		_cfgFileState = ::Assets::AssetState::Pending;

		CheckPreprocessInstalled();
	}

	static std::shared_ptr<TechniqueSharedResources> s_mainSharedResources = std::make_shared<TechniqueSharedResources>();

	TechniqueDelegate_Illum::TechniqueDelegate_Illum()
	: TechniqueDelegate_Illum(s_mainSharedResources)
	{
	}

	TechniqueDelegate_Illum::~TechniqueDelegate_Illum()
	{}

	::Assets::AssetState TechniqueDelegate_Illum::PrimeTechniqueCfg()
	{
		if (!_techniqueSetFuture) return _cfgFileState;

		auto actual = _techniqueSetFuture->TryActualize();
		if (!actual) {
			auto state = _techniqueSetFuture->GetAssetState();
			if (state == ::Assets::AssetState::Invalid) {
				_cfgFileDepVal = _techniqueSetFuture->GetDependencyValidation();
				_cfgFileState = ::Assets::AssetState::Invalid;
				_techniqueSetFuture.reset();
				return ::Assets::AssetState::Invalid;
			}
		}

		_cfgFileDepVal = actual->GetDependencyValidation();
		_cfgFileState = ::Assets::AssetState::Ready;
		_techniqueSetFuture.reset();
		const auto noPatchesHash = Hash64("NoPatches");
		const auto perPixelHash = Hash64("PerPixel");
		const auto perPixelAndEarlyRejectionHash = Hash64("PerPixelAndEarlyRejection");
		auto* noPatchesSrc = actual->FindEntry(noPatchesHash);
		auto* perPixelSrc = actual->FindEntry(perPixelHash);
		auto* perPixelAndEarlyRejectionSrc = actual->FindEntry(perPixelAndEarlyRejectionHash);
		if (!noPatchesSrc || !perPixelSrc || !perPixelAndEarlyRejectionSrc) {
			_cfgFileState = ::Assets::AssetState::Invalid;
			return ::Assets::AssetState::Invalid;
		}

		_noPatches = *noPatchesSrc;
		_perPixel = *perPixelSrc;
		_perPixelAndEarlyRejection = *perPixelAndEarlyRejectionSrc;

		return ::Assets::AssetState::Ready;
	}

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

		::Assets::FuturePtr<Technique> _techniqueSetFuture;
		::Assets::DepValPtr _cfgFileDepVal;
		::Assets::AssetState _cfgFileState;

		std::shared_ptr<TechniqueShaderVariationSet> _variationSet;

		::Assets::AssetState PrimeTechniqueCfg();
	};

	auto TechniqueDelegate_Legacy::Resolve(
		const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
		IteratorRange<const ParameterBox**> selectors,
		const RenderCore::Assets::RenderStateSet& input) -> ResolvedTechnique
	{
		if (PrimeTechniqueCfg() != ::Assets::AssetState::Ready)
			return {};

		assert(_variationSet);
		ResolvedTechnique result;
		result._shaderProgram = _variationSet->FindVariation(_techniqueIndex, selectors.begin());
		result._blend = _blend;
		result._rasterization = _rasterization;
		result._depthStencil = _depthStencil;
		return result;
	}

	::Assets::AssetState TechniqueDelegate_Legacy::PrimeTechniqueCfg()
	{
		if (!_techniqueSetFuture) return _cfgFileState;

		auto actual = _techniqueSetFuture->TryActualize();
		if (!actual) {
			auto state = _techniqueSetFuture->GetAssetState();
			if (state == ::Assets::AssetState::Invalid) {
				_cfgFileDepVal = _techniqueSetFuture->GetDependencyValidation();
				_cfgFileState = ::Assets::AssetState::Invalid;
				_techniqueSetFuture.reset();
				_variationSet.reset();
				return ::Assets::AssetState::Invalid;
			}
		}

		_cfgFileDepVal = actual->GetDependencyValidation();
		_cfgFileState = ::Assets::AssetState::Ready;
		_techniqueSetFuture.reset();

		_variationSet = std::make_shared<TechniqueShaderVariationSet>(actual);
		return ::Assets::AssetState::Ready;
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
		_techniqueSetFuture = ::Assets::MakeAsset<Technique>("xleres/Techniques/Illum.tech");
		_cfgFileState = ::Assets::AssetState::Pending;
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

	class TechniqueDelegatePrototype : public ITechniqueDelegate
	{
	public:
		ResolvedTechnique Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& input) override;

		TechniqueDelegatePrototype(
			const std::shared_ptr<TechniqueSetFile>& techniqueSet,
			const std::shared_ptr<TechniqueSharedResources>& sharedResources);
		~TechniqueDelegatePrototype();
	private:
		std::shared_ptr<TechniqueSharedResources> _sharedResources;

		std::shared_ptr<TechniqueSetFile> _techniqueSet;
		TechniqueEntry _noPatches;
		TechniqueEntry _perPixel;
		TechniqueEntry _perPixelAndEarlyRejection;
	};

	auto TechniqueDelegatePrototype::Resolve(
		const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
		IteratorRange<const ParameterBox**> selectors,
		const RenderCore::Assets::RenderStateSet& input) -> ResolvedTechnique
	{
		IteratorRange<const uint64_t*> patchExpansions = {};
		const TechniqueEntry* techEntry = &_noPatches;
		using IllumType = CompiledShaderPatchCollection::IllumDelegateAttachment::IllumType;
		switch (shaderPatches->_illumDelegate._type) {
		case IllumType::PerPixel:
			techEntry = &_perPixel;
			patchExpansions = MakeIteratorRange(s_patchExp_perPixel);
			break;
		case IllumType::PerPixelAndEarlyRejection:
			techEntry = &_perPixelAndEarlyRejection;
			patchExpansions = MakeIteratorRange(s_patchExp_perPixelAndEarlyRejection);
			break;
		default:
			break;
		}

		// todo -- combine all of the selectors into a single box in the technique object itself (rather than having to do a merge here)
		auto mergedTechniqueShaders = techEntry->_baseSelectors._selectors[0];
		for (unsigned c=1; c<dimof(techEntry->_baseSelectors._selectors); ++c)
			mergedTechniqueShaders.MergeIn(techEntry->_baseSelectors._selectors[c]);

		ShaderPatchFactory factory(*techEntry, shaderPatches.get(), patchExpansions);
		const auto& variation = _sharedResources->_mainVariationSet.FindVariation(
			selectors,
			mergedTechniqueShaders,
			shaderPatches->GetInterface().GetSelectorRelevance(),
			factory);

		ResolvedTechnique result;
		result._shaderProgram = variation._shaderFuture;
		// default render states for now
		return result;
	}

	TechniqueDelegatePrototype::TechniqueDelegatePrototype(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	: _sharedResources(sharedResources)
	, _techniqueSet(techniqueSet)
	{
		const auto noPatchesHash = Hash64("NoPatches");
		const auto perPixelHash = Hash64("PerPixel");
		const auto perPixelAndEarlyRejectionHash = Hash64("PerPixelAndEarlyRejection");
		auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
		auto* perPixelSrc = _techniqueSet->FindEntry(perPixelHash);
		auto* perPixelAndEarlyRejectionSrc = _techniqueSet->FindEntry(perPixelAndEarlyRejectionHash);
		if (!noPatchesSrc || !perPixelSrc || !perPixelAndEarlyRejectionSrc) {
			Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
		}

		_noPatches = *noPatchesSrc;
		_perPixel = *perPixelSrc;
		_perPixelAndEarlyRejection = *perPixelAndEarlyRejectionSrc;

		CheckPreprocessInstalled();
	}

	TechniqueDelegatePrototype::~TechniqueDelegatePrototype() {}

	std::shared_ptr<ITechniqueDelegate> CreateTechniqueDelegate(
		const std::shared_ptr<TechniqueSetFile>& techniqueSet,
		const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	{
		return std::make_shared<TechniqueDelegatePrototype>(techniqueSet, sharedResources);
	}

	ITechniqueDelegate::~ITechniqueDelegate() {}

}}

