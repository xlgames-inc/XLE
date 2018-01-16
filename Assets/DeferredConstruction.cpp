// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if 0

#include "DeferredConstruction.h"
#include "AssetUtils.h"

namespace Assets
{
	AssetState DeferredConstruction::GetAssetState() const
	{
		if (_upstreamMarker)
			return _upstreamMarker->GetAssetState();

		// If there no upstream marker, then we knew the state at construction.
		// It's either ready or invalid depending on whether there's an constructor
		// in our function table
		return _fns.IsEmpty() ? AssetState::Invalid : AssetState::Ready;
	}

	AssetState DeferredConstruction::StallWhilePending() const
	{
		if (_upstreamMarker)
			return _upstreamMarker->StallWhilePending();
		return _fns.IsEmpty() ? AssetState::Invalid : AssetState::Ready;
	}

	DeferredConstruction::~DeferredConstruction() {}
}

#endif


#include "../Utility/StringFormat.h"
#include "DivergentAsset.h"
#include "AssetServices.h"
#include "AssetSetManager.h"
#include "AssetHeap.h"
#include "../RenderCore/Assets/RawMaterial.h"
#include "ConfigFileContainer.h"

namespace Assets
{

	template<typename AssetType, typename... Params>
		std::shared_ptr<DivergentAsset<AssetType>> CreateDivergentAsset(Params... params)
	{
		auto& set = GetAssetSetManager().GetSetForType<AssetType>();

		auto originalFuture = set.Get(params...);
		originalFuture->StallWhilePending();

		auto divergentAsset = std::make_shared<DivergentAsset<AssetType>>(originalFuture->Actualize());
		auto workingAsset = divergentAsset->GetWorkingAsset();
		auto idInAssetHeap = set.SetShadowingAsset(std::move(workingAsset), params...);

		auto stringInitializer = Internal::AsString(params...);
		Services::GetDivergentAssetMan().AddDivergentAsset(
			typeid(AssetType).hash_code(), idInAssetHeap, stringInitializer,
			divergentAsset);

		return divergentAsset;
	}

	template std::shared_ptr<DivergentAsset<RenderCore::Assets::RawMaterial>> CreateDivergentAsset(StringSection<char>);

}

