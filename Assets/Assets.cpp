// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Assets.h"

#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../ConsoleRig/Log.h"
#include <sstream>

namespace Assets 
{
    namespace Internal
    {
        void LogHeader(unsigned count, const char typeName[])
        {
            LogInfo << "------------------------------------------------------------------------------------------";
            LogInfo << "    Asset set for type (" << typeName << ") with (" <<  count << ") items";
        }

        void LogAssetName(unsigned index, const char name[])
        {
            LogInfo << "    [" << index << "] " << name;
        }

        void InsertAssetName(   std::vector<std::pair<uint64, std::string>>& assetNames, 
                                uint64 hash, const std::string& name)
        {
            auto ni = LowerBound(assetNames, hash);
            if (ni != assetNames.cend() && ni->first == hash) {
                assert(0);  // hit a hash collision! In most cases, this will cause major problems!
                    // maybe this could happen if an asset is removed from the table, but it's name remains?
                ni->second = "<<collision>> " + ni->second + " <<with>> " + name;
            } else {
                assetNames.insert(ni, std::make_pair(hash, name));
            }
        }

    }
}
