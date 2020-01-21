// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"		// (for ResChar)
#include "../Utility/MemoryUtils.h"
#include <string>
#include <sstream>

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

		template<typename Param, typename std::enable_if<std::is_integral<Param>::value>::type* = nullptr>
			uint64_t HashParam(const Param& p, uint64_t seed) { return HashCombine(p, seed); }

		template<typename Param, decltype(std::declval<Param>().GetHash())* = nullptr>
			uint64_t HashParam(const Param& p, uint64_t seed) { return HashCombine(p.GetHash(), seed); }

		template<typename Param, decltype(std::declval<Param>()->GetHash())* = nullptr>
			uint64_t HashParam(const Param& p, uint64_t seed) { return HashCombine(p->GetHash(), seed); }

		template<typename Param, decltype(Hash64(std::declval<const Param&>(), 0ull))* = nullptr>
			uint64_t HashParam(const Param& p, uint64_t seed) { return Hash64(p, seed); }

		template <typename... Params>
			uint64_t BuildHash(Params... initialisers)
        { 
                //  Note Hash64 is a relatively expensive hash function
                //      ... we might get away with using a simpler/quicker hash function
                //  Note that if we move over to variadic template initialisers, it
                //  might not be as easy to build the hash value (because they would
                //  allow some initialisers to be different types -- not just strings).
                //  If we want to support any type as initialisers, we need to either
                //  define some rules for hashing arbitrary objects, or think of a better way
                //  to build the hash.
			uint64_t result = DefaultSeed64;
			int dummy[] = { 0, (result = HashParam(initialisers, result), 0)... };
			(void)dummy;
            return result;
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

		template <typename... Params>
			std::basic_string<ResChar> AsString(Params... initialisers);

		template<typename Asset, typename... Params>
			std::basic_string<ResChar> BuildDescriptiveName(Params... initialisers)
		{
			return Internal::AsString(initialisers...);
		}

	}
}

