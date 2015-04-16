// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "Detail/API.h"
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <memory>

namespace Utility
{
        ////////////   C O P Y  &  S E T   ////////////

    void     XlClearMemory          (void* p, size_t size);
    void     XlSetMemory            (void* p, int c, size_t size);

    void     XlCopyMemory           (void* dest, const void* src, size_t size);
    void     XlCopyMemoryAlign16    (void* dest, const void* src, size_t size);
    void     XlMoveMemory           (void* dest, const void* src, size_t size);

    int      XlCompareMemory        (const void* a, const void* b, size_t size);

    template <typename Type> 
        void XlZeroMemory(Type& destination)
            { XlClearMemory(&destination, sizeof(Type)); }

        // swap two memory regions
    template<int Count>
        inline void XlSwapMemory(void* a, void* b)
        {
            uint8 buf[Count];
            XlCopyMemory(buf, a, Count);
            XlCopyMemory(a, b, Count);
            XlCopyMemory(b, buf, Count);
        }

        ////////////   A L I G N E D   A L L O C A T E   ////////////

    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC

        #define XlMemAlign          _aligned_malloc
        #define XlMemAlignFree      _aligned_free

    #else

        inline void* XlMemAlign(size_t size, size_t alignment)
        {
            // void* result = nullptr;
            // int errorNumber = posix_memalign(&result, size, alignment);
            // assert(!errorNumber);
            // return result;
            return memalign(size, alignment);
        }
        
        inline void XlMemAlignFree(void* data)
        {
            free(data);
        }

    #endif

    struct PODAlignedDeletor { void operator()(void* p); };

    template<typename Type>
        struct AlignedDeletor : public PODAlignedDeletor 
    {
    public:
        void operator()(Type* p)
        {
            if (p) { p->~Type(); }
            PODAlignedDeletor::operator()(p);
        }
    };

        ////////////   H A S H I N G   ////////////

    static const uint64 DefaultSeed64 = 0xE49B0E3F5C27F17Eull;
    XL_UTILITY_API uint64 Hash64(const void* begin, const void* end, uint64 seed = DefaultSeed64);

    static const uint64 DefaultSeed32 = 0xB0F57EE3;
    XL_UTILITY_API uint32 Hash32(const void* begin, const void* end, uint32 seed = DefaultSeed32);

    XL_UTILITY_API uint64 Hash64(const char str[], uint64 seed = DefaultSeed64);
    XL_UTILITY_API uint64 Hash64(const std::string& str, uint64 seed = DefaultSeed64);

	inline uint64 HashCombine(uint64 high, uint64 low)
	{
		// This code based on "FarmHash"... which was in-turn
		// inspired by Murmur Hash. See:
		// https://code.google.com/p/farmhash/source/browse/trunk/src/farmhash.h
		// We want to combine two 64 bit hash values to create a new hash value
		// We could just return an xor of the two values. But this might not
		// be perfectly reliable (for example, the xor of 2 equals values is zero,
		// which could result in loss of information sometimes)
		const auto kMul = 0x9ddfea08eb382d69ull;
		auto a = (low ^ high) * kMul;
		a ^= (a >> 47);
		auto b = (high ^ a) * kMul;
		b ^= (b >> 47);
		b *= kMul;
		return b;
	}

        ////////////   C O M P I L E  -  T I M E  -  H A S H I N G   ////////////

    /// <summary>Generate a hash value at compile time</summary>
    /// Generate a simple hash value at compile time, from a set of 4-character 
    /// chars.
    ///
    /// The hash algorithm is very simple, and unique. It will produce very different
    /// hash values compared to the Hash64 function.
    /// There may be some value to attempting to make it match the "Hash64" function. 
    /// However, that would require a lot more work... The current implementation is
    /// more or less the simpliest possible implementation.
    ///
    /// Usage:
    /// <code>\code
    ///     static const uint64 HashValue = ConstHash64<'Skel', 'eton'>::Value
    /// \endcode</code>
    ///
    /// Note that a better implementation would be possible with C++11... But
    /// currently we're still supporting Visual Studio 2010, which doesn't have
    /// an implementation of constexpr.
    template<unsigned S0, unsigned S1 = 0, unsigned S2 = 0, unsigned S3 = 0>
        struct ConstHash64
    {
    public:
        template<unsigned NewValue, uint64 CumulativeHash>
            struct Calc
            {
                    // Here is the hash algorithm --
                    //  Since we're dealing 32 bit values, rather than chars, the algorithm
                    //  must be slightly difference. Here's something I've just invented.
                    //  It might be OK, but no real testing has gone into it.
                    //  Note that since we're building a 64 bit hash value, any strings with
                    //  8 or fewer characters can be stored in their entirety, anyway
                static const uint64 Value = (NewValue == 0) ? CumulativeHash : (((CumulativeHash << 21ull) | (CumulativeHash >> 43ull)) ^ uint64(NewValue));
            };

        static const uint64 Seed = 0xE49B0E3F5C27F17Eull;
        static const uint64 Value = Calc<S3, Calc<S2, Calc<S1, Calc<S0, Seed>::Value>::Value>::Value>::Value;
    };

        ////////////   I N L I N E   I M P L E M E N T A T I O N S   ////////////

    inline void XlClearMemory(void* p, size_t size)                     { memset(p, 0, size); }
    inline void XlSetMemory(void* p, int c, size_t size)                { memset(p, c, size); }
    inline int  XlCompareMemory(const void* a, const void* b, size_t size) { return memcmp(a, b, size); }
    inline void XlCopyMemory(void* dest, const void* src, size_t size)  { memcpy(dest, src, size); }
    inline void XlMoveMemory(void* dest, const void* src, size_t size)  { memmove(dest, src, size); }

    inline void XlCopyMemoryAlign16(void* dest, const void* src, size_t size)
    {
            //  Alignment promise on input & output
            //  (often used for textures, etc)
            //  Use 128bit instructions to improve copy speed...
        assert((size_t(dest) & 0xf)==0x0);
        assert((size_t(src) & 0xf)==0x0);
        memcpy(dest, src, size);
    }

    template<typename Type> std::unique_ptr<uint8[]> DuplicateMemory(const Type& input)
    {
        auto result = std::make_unique<uint8[]>(sizeof(Type));
        XlCopyMemory(result.get(), &input, sizeof(Type));
        return std::move(result);
    }

}

using namespace Utility;
