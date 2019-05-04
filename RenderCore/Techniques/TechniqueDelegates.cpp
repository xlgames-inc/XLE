// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TechniqueDelegates.h"
#include "DrawableMaterial.h"
#include "CompiledShaderPatchCollection.h"
#include "../Assets/RawMaterial.h"
#include "../Assets/LocalCompiledShaderSource.h"
#include "../Metal/Shader.h"
#include "../../ShaderParser/ShaderPatcher.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/Conversion.h"
#include <sstream>
#include <regex>

namespace RenderCore { namespace Techniques
{
	static const auto s_perPixel = Hash64("xleres/Nodes/Templates.sh::PerPixel");
	static const auto s_earlyRejectionTest = Hash64("xleres/Nodes/Templates.sh::EarlyRejectionTest");
	
	static bool HasReturn(const GraphLanguage::NodeGraphSignature&);

	class InstantiateShaderGraphPreprocessor : public RenderCore::Assets::ISourceCodePreprocessor
	{
	public:
		static SourceCodeWithRemapping AssembleShader(
			const RenderCore::Techniques::CompiledShaderPatchCollection& patchCollection,
			IteratorRange<const uint64_t*> redirectedPatchFunctions,
			const std::string& entryPointFileName)
		{
			// We can assemble the final shader in 3 fragments:
			//  1) the source code in CompiledShaderPatchCollection
			//  2) redirection functions (which redirect from the template function names to the concrete instantiations we want to tie in)
			//  3) include the entry point function itself

			std::stringstream output;

			output << patchCollection.GetSourceCode();

			for (auto fn:redirectedPatchFunctions) {
				auto i = std::find_if(
					patchCollection.GetPatches().begin(), patchCollection.GetPatches().end(),
					[fn](const RenderCore::Techniques::CompiledShaderPatchCollection::Patch& p) { return p._implementsHash == fn; });
				assert(i!=patchCollection.GetPatches().end());
				if (i == patchCollection.GetPatches().end()) {
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
			output << "#include \'" << entryPointFileName << "\"" << std::endl;

			SourceCodeWithRemapping result;
			result._processedSource = output.str();
			/*result._dependencies.insert(
				result._dependencies.end(),
				patchCollection._dependencies.begin(), patchCollection._dependencies.end());*/

			// We could fill in the _lineMarkers member with some line marker information
			// from the original shader graph compile; but that might be overkill

			return result;
		}

		virtual SourceCodeWithRemapping RunPreprocessor(const char filename[])
		{
			// Encoded in the filename is the guid for the CompiledShaderPatchCollection, the list of functions that require
			// redirection and the entry point shader filename
			
			static std::regex filenameExp(R"--(([^-]+)-([0-9,a-f,A-F]{1,16})(?:-([0-9,a-f,A-F]{1,16}))*)--");
			std::cmatch matches;
			if (!std::regex_match(filename, XlStringEnd(filename), matches, filenameExp) || matches.size() < 3)
				return {};		// don't understand the input filename, we can't expand this

			uint64_t patchCollectionGuid = Conversion::Convert<uint64_t>(MakeStringSection(matches[2].first, matches[2].second));
			auto i = LowerBound(_patchCollections, patchCollectionGuid);
			if (i == _patchCollections.end() || i->first != patchCollectionGuid)
				return {};		// the patch collection hasn't been registered before hand!

			std::vector<uint64_t> redirectedPatchFunctions;
			redirectedPatchFunctions.reserve(matches.size() - 3);
			for (auto m=matches.begin()+3; m!=matches.end(); ++m)
				redirectedPatchFunctions.push_back(Conversion::Convert<uint64_t>(MakeStringSection(m->first, m->second)));

			return AssembleShader(*i->second, MakeIteratorRange(redirectedPatchFunctions), filename);
		}

		void RegisterPatchCollection(const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollection)
		{
			auto guid = patchCollection->GetGUID();
			auto e = LowerBound(_patchCollections, guid);
			if (e == _patchCollections.end() || e->first != guid) {
				_patchCollections.push_back(std::make_pair(guid, patchCollection));
			} else {
				// We're trying to add a patch collection that matches another already registered one with
				// the same guid.
				assert(e->second == patchCollection);
			}
		}

		InstantiateShaderGraphPreprocessor() {}
		~InstantiateShaderGraphPreprocessor() {}

	private:
		std::vector<std::pair<uint64_t, std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>>> _patchCollections;
	};

	class ShaderPatchFactory : public IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines)
		{
			std::stringstream vsName;
			vsName << _entry->_vertexShaderName << "-" << std::hex << _patchCollection->GetGUID();
			for (auto exp:_patchExpansions) vsName << "-" << exp;

			if (_entry->_geometryShaderName.empty()) {
				return ::Assets::MakeAsset<Metal::ShaderProgram>(_entry->_vertexShaderName, _entry->_pixelShaderName, defines);
			} else {
				return ::Assets::MakeAsset<Metal::ShaderProgram>(_entry->_vertexShaderName, _entry->_geometryShaderName, _entry->_pixelShaderName, defines);
			}
		}

		ShaderPatchFactory(
			const TechniqueEntry& techEntry, 
			const RenderCore::Techniques::CompiledShaderPatchCollection* patchCollection,
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
		const RenderCore::Techniques::CompiledShaderPatchCollection* _patchCollection;
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

	TechniqueDelegate_Illum::TechniqueDelegate_Illum(const std::shared_ptr<TechniqueSharedResources>& sharedResources)
	: _sharedResources(sharedResources)
	{
		_techniqueSetFuture = ::Assets::MakeAsset<TechniqueSetFile>("xleres/Techniques/New/Illum.tech");
		_cfgFileState = ::Assets::AssetState::Pending;
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

}}

