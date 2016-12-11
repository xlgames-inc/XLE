// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetTraits.h"
#include "Assets.h"
#include "ConfigFileContainer.h"

namespace Assets
{
	const ConfigFileContainer<>& GetAssetContainer(const ResChar identifier[])
	{
		return GetAsset<ConfigFileContainer<>>(identifier);
	}
}

