// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"		// (for ResChar)
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"
#include <assert.h>

namespace Utility
{
	template<typename CharType> class InputStreamFormatter;
}

namespace Assets
{
	template <typename Asset> class DivergentAsset;
	template <typename Formatter> class ConfigFileContainer;
	class DirectorySearchRules;
	class ChunkFileContainer;
	class AssetChunkRequest;
    class IArtifact;

	namespace Internal
	{
		template <typename AssetType>
			class AssetTraits
		{
		private:
			struct HasCompileProcessTypeHelper
			{
				struct FakeBase { static const uint64_t CompileProcessType; };
				struct TestSubject : public FakeBase, public AssetType {};

				template <typename C, C> struct Check;

				// This technique is based on an implementation from StackOverflow. Here, taking the address
				// of the static member variable in TestSubject would be ambiguous, iff CompileProcessType 
				// is actually a member of AssetType (otherwise, the member in FakeBase is found)
				template <typename C> static std::false_type Test(Check<const uint64_t*, &C::CompileProcessType> *);
				template <typename> static std::true_type Test(...);

				static const bool value = decltype(Test<TestSubject>(0))::value;
			};

			template<typename T> static auto HasChunkRequestsHelper(int) -> decltype(&T::ChunkRequests[0], std::true_type{});
			template<typename...> static auto HasChunkRequestsHelper(...) -> std::false_type;

		public:
			using DivAsset = DivergentAsset<AssetType>;

			static const bool Constructor_Formatter = std::is_constructible<AssetType, InputStreamFormatter<utf8>&, const DirectorySearchRules&, const DepValPtr&>::value;
			static const bool Constructor_ChunkFileContainer = std::is_constructible<AssetType, const ChunkFileContainer&>::value;

			static const bool HasCompileProcessType = HasCompileProcessTypeHelper::value;
			static const bool HasChunkRequests = decltype(HasChunkRequestsHelper<AssetType>(0))::value;
		};

			///////////////////////////////////////////////////////////////////////////////////////////////////

		const ConfigFileContainer<InputStreamFormatter<utf8>>& GetConfigFileContainer(StringSection<ResChar> identifier);
		const ChunkFileContainer& GetChunkFileContainer(StringSection<ResChar> identifier);

        template <typename... Params> uint64 BuildHash(Params... initialisers);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	#define ENABLE_IF(X) typename std::enable_if<X>::type* = nullptr

	template<typename AssetType, ENABLE_IF(Internal::AssetTraits<AssetType>::Constructor_Formatter)>
		std::unique_ptr<AssetType> AutoConstructAsset(StringSection<ResChar> initializer)
	{
		// First parameter should be the section of the input file to read (or just use the root of the file if it doesn't exist)
		const char* p = XlFindChar(initializer, ':');
		if (p) {
			char buffer[256];
			XlCopyString(buffer, MakeStringSection(initializer.begin(), p));
			const auto& container = Internal::GetConfigFileContainer(buffer);
			auto fmttr = container.GetFormatter(MakeStringSection((const utf8*)(p+1), (const utf8*)initializer.end()));
			return std::make_unique<AssetType>(
				fmttr, 
				DefaultDirectorySearchRules(buffer),
				container.GetDependencyValidation());
		} else {
			const auto& container = Internal::GetConfigFileContainer(initializer);
			auto fmttr = container.GetRootFormatter();
			return std::make_unique<AssetType>(
				fmttr,
				DefaultDirectorySearchRules(initializer),
				container.GetDependencyValidation());
		}
	}

	template<typename AssetType, ENABLE_IF(Internal::AssetTraits<AssetType>::Constructor_Formatter)>
		std::unique_ptr<AssetType> AutoConstructAsset(const Blob& blob, const DepValPtr& depVal, StringSection<ResChar> requestParameters = {})
	{
		auto container = ConfigFileContainer<>(blob, depVal);
		auto fmttr = requestParameters.IsEmpty() ? container.GetRootFormatter() : container.GetFormatter(MakeStringSection((const utf8*)requestParameters.begin(), (const utf8*)requestParameters.end()));
		return std::make_unique<AssetType>(
			fmttr,
			DirectorySearchRules{},
			container.GetDependencyValidation());
	}
	
	template<typename AssetType, typename... Params, ENABLE_IF(Internal::AssetTraits<AssetType>::Constructor_ChunkFileContainer)>
		std::unique_ptr<AssetType> AutoConstructAsset(StringSection<ResChar> initializer)
	{
		const auto& container = Internal::GetChunkFileContainer(initializer);
		return std::make_unique<AssetType>(container);
	}

	template<typename AssetType, typename... Params, ENABLE_IF(Internal::AssetTraits<AssetType>::Constructor_ChunkFileContainer)>
		std::unique_ptr<AssetType> AutoConstructAsset(const Blob& blob, const DepValPtr& depVal, StringSection<ResChar> requestParameters = {})
	{
		return std::make_unique<AssetType>(ChunkFileContainer(blob, depVal, requestParameters));
	}

	template<typename AssetType, typename... Params, ENABLE_IF(Internal::AssetTraits<AssetType>::HasChunkRequests)>
		std::unique_ptr<AssetType> AutoConstructAsset(StringSection<ResChar> initializer)
	{
		const auto& container = Internal::GetChunkFileContainer(initializer);
		auto chunks = container.ResolveRequests(MakeIteratorRange(AssetType::ChunkRequests));
		return std::make_unique<AssetType>(MakeIteratorRange(chunks), container.GetDependencyValidation());
	}

	template<typename AssetType, typename... Params, ENABLE_IF(Internal::AssetTraits<AssetType>::HasChunkRequests)>
		std::unique_ptr<AssetType> AutoConstructAsset(const Blob& blob, const DepValPtr& depVal, StringSection<ResChar> requestParameters = {})
	{
		auto chunks = ChunkFileContainer(blob, depVal, requestParameters).ResolveRequests(MakeIteratorRange(AssetType::ChunkRequests));
		return std::make_unique<AssetType>(MakeIteratorRange(chunks), depVal);
	}

	template<typename AssetType, typename... Params, typename std::enable_if<std::is_constructible<AssetType, Params...>::value>::type* = nullptr>
		static std::unique_ptr<AssetType> AutoConstructAsset(Params... initialisers)
	{
		return std::make_unique<AssetType>(std::forward<Params>(initialisers)...);
	}

	#undef ENABLE_IF

}

