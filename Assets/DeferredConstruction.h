// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetsCore.h"
#include "IntermediateAssets.h"
#include "../Utility/FunctionUtils.h"
#include <memory>

namespace Assets
{
	class PendingOperationMarker;

	class DeferredConstruction
	{
	public:
		template<typename Type>
			std::unique_ptr<Type> PerformConstructor();

		AssetState GetAssetState() const;
		AssetState StallWhilePending() const;
		const DepValPtr& GetDependencyValidation() const { return _depVal; }

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

	namespace Internal 
	{
		void TryRecompile(const Assets::Exceptions::FormatError&); 

		template<typename AssetType, typename std::enable_if<Internal::AssetTraits<AssetType>::Constructor_IntermediateAssetLocator>::type* = nullptr>
			static std::unique_ptr<AssetType> ConstructFromIntermediateAssetLocator(const IntermediateAssetLocator& locator, const ResChar initializer[])
			{
				return std::make_unique<AssetType>(locator, initializer);
			}

		template<typename AssetType, typename std::enable_if<!Internal::AssetTraits<AssetType>::Constructor_IntermediateAssetLocator>::type* = nullptr>
			static std::unique_ptr<AssetType> ConstructFromIntermediateAssetLocator(const IntermediateAssetLocator& locator, const ResChar initializer[])
			{
				return AutoConstructAsset<AssetType>(locator._sourceID0);
			}
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

		auto existingLoc = marker->GetExistingAsset();
		if (!existingLoc._dependencyValidation || existingLoc._dependencyValidation->GetValidationIndex()!=0) {
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
					return Internal::ConstructFromIntermediateAssetLocator<AssetType>(pendingCompile->GetLocator(), init0.c_str());
				});
			return std::make_shared<DeferredConstruction>(pendingCompile, pendingCompile->GetLocator()._dependencyValidation, std::move(constructorCallback));
		} else {
			std::function<std::unique_ptr<AssetType>()> constructorCallback(
				[existingLoc, init0]() -> std::unique_ptr<AssetType> { 
					// Since we're not explicitly executing the compile in this case, we should check for
					// cases were we should invoke a recompile
					TRY { return Internal::ConstructFromIntermediateAssetLocator<AssetType>(existingLoc, init0.c_str()); }
					CATCH (const Exceptions::FormatError& e) { Internal::TryRecompile(e); throw; } 
					CATCH_END;
				});
			return std::make_shared<DeferredConstruction>(nullptr, existingLoc._dependencyValidation, std::move(constructorCallback));
		}
	}
}

