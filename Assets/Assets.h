// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "AssetsInternal.h"
#include "../Utility/Streams/FileSystemMonitor.h"       // (for OnChangeCallback base class)

namespace Assets
{

    template<typename AssetType, typename... Params> const AssetType& GetAsset(Params... initialisers)		    { return Internal::GetAsset<false, true, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetDep(Params... initialisers)	    { return Internal::GetAsset<true, true, AssetType>(std::forward<Params>(initialisers)...); }
    template<typename AssetType, typename... Params> const AssetType& GetAssetComp(Params... initialisers)	    { return Internal::GetAsset<true, true, AssetType>(std::forward<Params>(initialisers)...); }

    template<typename AssetType, typename... Params> 
        std::shared_ptr<typename Internal::AssetTraits<AssetType>::DivAsset>& GetDivergentAsset(Params... initialisers)	
            { return Internal::GetDivergentAsset<AssetType, false>(std::forward<Params>(initialisers)...); }

    template<typename AssetType, typename... Params> 
        std::shared_ptr<typename Internal::AssetTraits<AssetType>::DivAsset>& GetDivergentAssetComp(Params... initialisers)	
            { return Internal::GetDivergentAsset<AssetType, true>(std::forward<Params>(initialisers)...); }

}

namespace Assets 
{
    namespace Internal
    {
        template <typename Object>
			inline void StreamCommaSeparated(std::basic_stringstream<ResChar>& result, const Object& obj)
		{
			result << ", " << obj;
		}

		template <typename P0, typename... Params>
			std::basic_string<ResChar> AsString(P0 p0, Params... initialisers)
		{
			std::basic_stringstream<ResChar> result;
            result << p0;
			int dummy[] = { 0, (StreamCommaSeparated(result, initialisers), 0)... };
			(void)dummy;
			return result.str();
		}

		template <typename... Params>
			uint64 BuildHash(Params... initialisers)
        { 
                //  Note Hash64 is a relatively expensive hash function
                //      ... we might get away with using a simpler/quicker hash function
                //  Note that if we move over to variadic template initialisers, it
                //  might not be as easy to build the hash value (because they would
                //  allow some initialisers to be different types -- not just strings).
                //  If we want to support any type as initialisers, we need to either
                //  define some rules for hashing arbitrary objects, or think of a better way
                //  to build the hash.
            uint64 result = DefaultSeed64;
			int dummy[] = { 0, (result = Hash64(initialisers, result), 0)... };
			(void)dummy;
            return result;
        }
    }
}


