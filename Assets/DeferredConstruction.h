// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
// #include "Assets.h"
// #include "AssetsInternal.h"
#include "IntermediateAssets.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include <memory>

namespace Assets
{
	class PendingOperationMarker;
    class ICompileMarker;
    namespace Internal
    {
        template <typename AssetType> class AssetTraits;
        std::shared_ptr<ICompileMarker> BeginCompileOperation(uint64 typeCode, const StringSection<ResChar> initializers[], unsigned initializerCount);
    }

	class DeferredConstruction
	{
	public:
		template<typename Type>
			std::unique_ptr<Type> PerformConstructor();

		AssetState GetAssetState() const;
		AssetState StallWhilePending() const;
		const DepValPtr& GetDependencyValidation() const { return _depVal; }

		const VariantFunctions& GetVariants() const { return _fns; }
		VariantFunctions& GetVariants() { return _fns; }

		template<typename Type>
			DeferredConstruction(
				const std::shared_ptr<PendingOperationMarker>& upstream, 
				const DepValPtr& depVal,
				std::function<std::unique_ptr<Type>()>&& constructor);
		~DeferredConstruction();
	private:
		VariantFunctions _fns;
		std::shared_ptr<PendingOperationMarker> _upstreamMarker;
		DepValPtr _depVal;
	};

	template<typename Type>
		std::unique_ptr<Type> DeferredConstruction::PerformConstructor() { return _fns.Call<std::unique_ptr<Type>>(typeid(Type).hash_code()); }

	template<typename Type>
		DeferredConstruction::DeferredConstruction(
			const std::shared_ptr<PendingOperationMarker>& upstream, 
			const DepValPtr& depVal,
			std::function<std::unique_ptr<Type>()>&& constructor)
			: _upstreamMarker(upstream), _depVal(depVal) 
		{
			if (!_depVal)
				_depVal = std::make_shared<DependencyValidation>();
			_fns.Add<std::unique_ptr<Type>()>(typeid(Type).hash_code(), std::move(constructor));
		}

	////////////////////////////////////////////////////////////////////////////////////////////////

	template<	typename AssetType, 
				typename... Args,
				typename std::enable_if<std::is_constructible<AssetType, const IArtifact&, Args...>::value>::type* = nullptr>
		std::unique_ptr<AssetType> AutoConstructAsset(const IArtifact& locator, Args... args)
		{
			return std::make_unique<AssetType>(locator, std::forward<Args>(args)...);
		}

	template<typename AssetType>
		static std::shared_ptr<DeferredConstruction> DefaultBeginDeferredConstruction(
			const StringSection<ResChar> initializers[], unsigned initializerCount,
			uint64 compileTypeCode = GetCompileProcessType<AssetType>())
	{
		// Begin a compilation operation via the registered compilers for this type.
		// Our deferred constructor will wait for the completion of that compilation operation,
		// and then construct the final asset from the result

		auto marker = Internal::BeginCompileOperation(compileTypeCode, initializers, initializerCount);
		std::basic_string<ResChar> init0 = initializers[0].AsString();

		// Attempt to load the existing asset immediate. If we get an unsupported version error, we will attempt a recompile

		auto existingLoc = marker->GetExistingAsset();
		if (existingLoc->GetDependencyValidation() && existingLoc->GetDependencyValidation()->GetValidationIndex()==0) {

			std::unique_ptr<AssetType> asset = nullptr;
			// Attempt recompile if we catch InvalidAsset or a FormatError with UnsupportedVersion or an IOException with FileNotFound
			TRY {
				asset = AutoConstructAsset<AssetType>(*existingLoc, MakeStringSection(init0));
			} CATCH (const Exceptions::InvalidAsset&) {
			} CATCH (const Exceptions::FormatError& e) {
				if (e.GetReason() != ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion)
					throw;
			} CATCH(const Utility::Exceptions::IOException& e) {
				if (e.GetReason() != Utility::Exceptions::IOException::Reason::FileNotFound)
					throw;
			} CATCH_END

			if (asset) {
				// (awkward shared_ptr wrapper required here to get around difficulties moving variables into lambdas and move-only std::function<> objects
				std::shared_ptr<std::unique_ptr<AssetType>> wrapper = std::make_shared<std::unique_ptr<AssetType>>(std::move(asset));
				std::function<std::unique_ptr<AssetType>()> constructorCallback([wrapper]() -> std::unique_ptr<AssetType> { return std::move(*wrapper.get()); });
				return std::make_shared<DeferredConstruction>(nullptr, existingLoc->GetDependencyValidation(), std::move(constructorCallback));
			}

			// If we didn't get a valid asset, we will fall through and attempt recompile...

		} ////////////////////////////////////////////////////////////////////////////////////////////////

		// no existing asset (or out-of-date) -- we must invoke a compile
		auto pendingCompile = marker->InvokeCompile();
		std::function<std::unique_ptr<AssetType>()> constructorCallback(
			[pendingCompile, init0]() -> std::unique_ptr<AssetType> {
				auto state = pendingCompile->GetAssetState();
				if (state == AssetState::Pending)
					Throw(Exceptions::PendingAsset(init0.c_str(), "Pending compilation operation"));
				if (state == AssetState::Invalid)
					Throw(Exceptions::InvalidAsset(init0.c_str(), "Failure during compilation operation"));
				assert(state == AssetState::Ready);
				return AutoConstructAsset<AssetType>(*pendingCompile->GetArtifacts()[0].second, MakeStringSection(init0));
			});
        assert(0);      // todo - need to set the dependncy validation to something reasonable -- 
		return std::make_shared<DeferredConstruction>(pendingCompile, nullptr, std::move(constructorCallback));
	}
}

