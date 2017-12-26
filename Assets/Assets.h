#pragma once

#include "AssetSetManager.h"
#include "AssetFuture.h"
#include "AssetHeap.h"

namespace Assets
{

	template<typename AssetType, typename... Params>
		std::shared_ptr<AssetFuture<AssetType>> MakeAsset(Params... initialisers)
	{
		return GetAssetSetManager().GetSetForType<AssetType>().Get(std::forward<Params>(initialisers)...);
	}

	template<typename AssetType, typename... Params>
		const AssetType& Actualize(Params... initialisers)
	{
		auto future = MakeAsset<AssetType>(initialisers...);
		future->StallWhilePending();
		return *future->Actualize();
	}

	template<typename AssetType, typename... Params>
		const AssetType& GetAsset(Params... initialisers) { return Actualize<AssetType>(std::forward<Params>(initialisers)...); }

	template<typename AssetType, typename... Params>
		const AssetType& GetAssetDep(Params... initialisers) { return Actualize<AssetType>(std::forward<Params>(initialisers)...); }

	template<typename AssetType, typename... Params>
		const AssetType& GetAssetComp(Params... initialisers) { return Actualize<AssetType>(std::forward<Params>(initialisers)...); }
		
}

