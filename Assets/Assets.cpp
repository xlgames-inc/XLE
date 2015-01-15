// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Assets.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../ConsoleRig/Log.h"

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
                                uint64 hash,
                                const char* initializers[], unsigned initializerCount)
        {
            std::stringstream name;
            for (unsigned c=0; c<initializerCount; ++c) {
                auto s = initializers[c];
                if (c != 0) { name << " "; }
                if (s) { name << "[" << s << "]"; }
                else { name << "[null]"; }
            }
            auto ni = LowerBound(assetNames, hash);
            if (ni != assetNames.cend() && ni->first == hash) {
                assert(0);  // hit a hash collision! In most cases, this will cause major problems!
                    // maybe this could happen if an asset is removed from the table, but it's name remains?
                ni->second = "<<collision>> " + ni->second + " <<with>> " + name.str();
            } else {
                assetNames.insert(ni, std::make_pair(hash, name.str()));
            }
        }

        template <int InitCount>
            uint64 BuildHash(AssetInitializer<InitCount> init)
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
                for (unsigned c=0; c<InitCount; ++c) {
                    auto s = ((const char**)&init)[c];
                    result = s?Hash64(s, &s[Utility::XlStringLen(s)], result):result;  // (previous hash becomes seed for next one)
                }
                return result;
            }

        template uint64 BuildHash(AssetInitializer<1> init);
        template uint64 BuildHash(AssetInitializer<2> init);
        template uint64 BuildHash(AssetInitializer<3> init);
        template uint64 BuildHash(AssetInitializer<4> init);
        template uint64 BuildHash(AssetInitializer<5> init);
        template uint64 BuildHash(AssetInitializer<6> init);
    }
}

