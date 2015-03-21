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

		template <typename Object>
			inline void StreamCommaSeparated(std::stringstream& result, const Object& obj)
		{
			result << obj << ", ";
		}

		template <typename... Params>
			std::string AsString(Params... initialisers)
		{
			std::stringstream result;
			int dummy[] = { 0, (StreamCommaSeparated(result, initialisers), 0)... };
			(void)dummy;
			// StreamCommaSeparated(result, initialisers)...;
			return result.str();
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
			// (result = Hash64(initialisers, result))...;
            return result;
        }

            // the following isn't going to work... we can't predict all of the expansions that will be used
		template std::basic_string<ResChar> AsString(const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);

		template uint64 BuildHash(const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*, const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);
		template uint64 BuildHash(const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*, const ResChar*);

        template std::basic_string<ResChar> AsString(ResChar*); 
        template uint64 BuildHash(ResChar*);

        template std::basic_string<ResChar> AsString(const ResChar*, ResChar*);
        template uint64 BuildHash(const ResChar*, ResChar*);

        template std::basic_string<ResChar> AsString(const ResChar*, const ResChar*, ResChar*);
        template uint64 BuildHash(const ResChar*, const ResChar*, ResChar*);

        template std::basic_string<ResChar> AsString(char const *, char const *, char const *, char const *, char const *, char *);
        template uint64 BuildHash(char const *, char const *, char const *, char const *, char const *, char *);

        template std::basic_string<ResChar> AsString(char *, char const *);
        template uint64 BuildHash(char *, char const *);

        template std::basic_string<ResChar> AsString(char const *, char const *, char const *, char *);
        template uint64 BuildHash(char const *, char const *, char const *, char *);

    }
}

