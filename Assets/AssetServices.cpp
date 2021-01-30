// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetServices.h"
#include "AssetSetManager.h"
#include "CompileAndAsyncManager.h"
#include "../ConsoleRig/AttachablePtr.h"

namespace Assets
{
    static ConsoleRig::AttachablePtr<AssetSetManager> s_assetSetsManagerInstance;
    static ConsoleRig::AttachablePtr<CompileAndAsyncManager> s_compileAndAsyncManager;

    AssetSetManager& Services::GetAssetSets()
    {
        return *s_assetSetsManagerInstance;
    }

    CompileAndAsyncManager& Services::GetAsyncMan()
    {
        return *s_compileAndAsyncManager;
    }

    bool Services::HasAssetSets()
    {
        return s_assetSetsManagerInstance != nullptr;
    }
}

