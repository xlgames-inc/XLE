// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetTraits.h"
#include "Assets.h"
#include "ConfigFileContainer.h"
#include "ChunkFileContainer.h"

namespace Assets { namespace Internal
{
	const ConfigFileContainer<>& GetConfigFileContainer(const ResChar identifier[])
	{
		return ::Assets::GetAsset<ConfigFileContainer<>>(identifier);
	}

	const ChunkFileContainer& GetChunkFileContainer(const ResChar identifier[])
	{
		return ::Assets::GetAsset<ChunkFileContainer>(identifier);
	}
}}

