// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "../Utility/UTFUtils.h"
#include <assert.h>

namespace Utility
{
	template<typename CharType> class InputStreamFormatter;
}

namespace Assets
{
	template <typename Asset> class DivergentAsset;
	template <typename Formatter> class ConfigFileContainer;
	class DeferredConstruction;
	class DirectorySearchRules;
	class ChunkFileContainer;

	namespace Internal
	{
		template <typename AssetType>
			class AssetTraits
		{
		private:
			template<typename T> struct HasGetAssetStateHelper
			{
				template<typename U, AssetState (U::*)() const> struct FunctionSignature {};
				template<typename U> static std::true_type Test1(FunctionSignature<U, &U::GetAssetState>*);
				template<typename U> static std::false_type Test1(...);
				static const bool Result = decltype(Test1<T>(0))::value;
			};

			template<typename T> struct HasBeginDeferredConstructionHelper
			{
				template<typename U, std::shared_ptr<DeferredConstruction> (*)(const ResChar*[], unsigned)> struct FunctionSignature {};
				template<typename U> static std::true_type Test1(FunctionSignature<U, &U::BeginDeferredConstruction>*);
				template<typename U> static std::false_type Test1(...);
				static const bool Result = decltype(Test1<T>(0))::value;
			};

		public:
			using DivAsset = DivergentAsset<AssetType>;

			static const bool Constructor_DeferredConstruction = std::is_constructible<AssetType, const std::shared_ptr<DeferredConstruction>&>::value;
			static const bool Constructor_Formatter = std::is_constructible<AssetType, InputStreamFormatter<utf8>&, const DirectorySearchRules&, const DepValPtr&>::value;
			static const bool Constructor_IntermediateAssetLocator = std::is_constructible<AssetType, const IntermediateAssetLocator&, const ResChar[]>::value;
			static const bool Constructor_ChunkFileContainer = std::is_constructible<AssetType, const ChunkFileContainer&>::value;

			static const bool HasBeginDeferredConstruction = HasBeginDeferredConstructionHelper<AssetType>::Result;
			static const bool HasGetAssetState = HasGetAssetStateHelper<AssetType>::Result;
		};

			///////////////////////////////////////////////////////////////////////////////////////////////////

		const ConfigFileContainer<InputStreamFormatter<utf8>>& GetConfigFileContainer(const ResChar identifier[]);
		const ChunkFileContainer& GetChunkFileContainer(const ResChar identifier[]);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename AssetType, typename std::enable_if<Internal::AssetTraits<AssetType>::Constructor_Formatter>::type* = nullptr>
		static std::unique_ptr<AssetType> AutoConstructAsset(const ResChar initializer[])
	{
		// First parameter should be the section of the input file to read (or just use the root of the file if it doesn't exist)
		const char* p = XlFindChar(initializer, ':');
		if (p) {
			char buffer[256];
			XlCopyString(buffer, MakeStringSection(initializer, p));
			const auto& container = Internal::GetConfigFileContainer(buffer);
			auto fmttr = container.GetFormatter((const utf8*)(p+1));
			return std::make_unique<AssetType>(
				fmttr, 
				DefaultDirectorySearchRules(buffer),
				container.GetDependencyValidation());
		} else {
			const auto& container = Internal::GetConfigFileContainer(initializer);
			auto fmttr = container.GetFormatter((const utf8*)(p+1));
			return std::make_unique<AssetType>(
				fmttr,
				DefaultDirectorySearchRules(initializer),
				container.GetDependencyValidation());
		}
	}
	
	template<typename AssetType, typename... Params, typename std::enable_if<Internal::AssetTraits<AssetType>::Constructor_ChunkFileContainer>::type* = nullptr>
		static std::unique_ptr<AssetType> AutoConstructAsset(const ResChar initializer[])
	{
		const auto& container = Internal::GetChunkFileContainer(initializer);
		return std::make_unique<AssetType>(container);
	}

	template<typename AssetType, typename... Params, typename std::enable_if<!Internal::AssetTraits<AssetType>::Constructor_Formatter && !Internal::AssetTraits<AssetType>::Constructor_ChunkFileContainer>::type* = nullptr>
		static std::unique_ptr<AssetType> AutoConstructAsset(Params... initialisers)
	{
		return std::make_unique<AssetType>(std::forward<Params>(initialisers)...);
	}

	template<
        typename AssetType, typename... Params, 
        typename std::enable_if<Internal::AssetTraits<AssetType>::Constructor_DeferredConstruction && Internal::AssetTraits<AssetType>::HasBeginDeferredConstruction>::type* = nullptr>
        typename std::unique_ptr<AssetType> AutoConstructAssetDeferred(Params... initialisers)
    {
		auto hash = Internal::BuildHash(initialisers...);
		std::shared_ptr<DeferredConstruction> deferredConstruction;
		{
			auto assetSet = Internal::GetAssetSet<AssetType>();
			auto i = LowerBound(assetSet->_deferredConstructions, hash);
			if (i == assetSet->_deferredConstructions.end() || i->first != hash) {
				const char* inits[] = { ((const char*)initialisers)... };
				deferredConstruction = AssetType::BeginDeferredConstruction(inits, dimof(inits));
				i = assetSet->_deferredConstructions.insert(i, std::make_pair(hash, deferredConstruction));
			} else {
				deferredConstruction = i->second;
			}
		}

		return std::make_unique<AssetType>(deferredConstruction);
    }

	template<
        typename AssetType, typename... Params, 
        typename std::enable_if<!Internal::AssetTraits<AssetType>::Constructor_DeferredConstruction && Internal::AssetTraits<AssetType>::HasBeginDeferredConstruction>::type* = nullptr>
        typename std::unique_ptr<AssetType> AutoConstructAssetDeferred(Params... initialisers)
    {
		auto hash = Internal::BuildHash(initialisers...);
		std::shared_ptr<DeferredConstruction> deferredConstruction;
		{
			auto assetSet = Internal::GetAssetSet<AssetType>();
			auto i = LowerBound(assetSet->_deferredConstructions, hash);
			if (i == assetSet->_deferredConstructions.end() || i->first != hash) {
				const char* inits[] = { ((const char*)initialisers)... };
				deferredConstruction = AssetType::BeginDeferredConstruction(inits, dimof(inits));
				i = assetSet->_deferredConstructions.insert(i, std::make_pair(hash, deferredConstruction));
			} else {
				deferredConstruction = i->second;
			}
		}

		auto state = deferredConstruction->GetAssetState();
		const auto* initializer = (const char*)std::get<0>(std::tuple<Params...>(initialisers...));
		if (state == AssetState::Pending)
			Throw(Exceptions::PendingAsset(initializer, "Pending deferred construction"));
		if (state == AssetState::Invalid)
			Throw(Exceptions::PendingAsset(initializer, "Invalid during deferred construction"));
		assert(state == AssetState::Ready);
		return deferredConstruction->PerformConstructor<AssetType>();
    }

	template<
		typename AssetType, typename... Params, 
		typename std::enable_if<!Internal::AssetTraits<AssetType>::HasBeginDeferredConstruction>::type* = nullptr>
		typename std::unique_ptr<AssetType> AutoConstructAssetDeferred(Params... initialisers)
	{
		return AutoConstructAsset<AssetType>(std::forward<Params>(initialisers)...);
	}
}

