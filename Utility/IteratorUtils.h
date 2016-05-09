// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PtrUtils.h"       // for AsPointer
#include <vector>
#include <algorithm>

namespace Utility
{
        //
        //  This file contains some utilities for common STL operations.
        //  Avoid adding too many utilities to this file, because it's included
        //  in many places (including through Assets.h, which propagates through
        //  a lot)
        //  
    template <typename First, typename Second>
        class CompareFirst
        {
        public:
            inline bool operator()(const std::pair<First, Second>& lhs, const std::pair<First, Second>& rhs) const   { return lhs.first < rhs.first; }
            inline bool operator()(const std::pair<First, Second>& lhs, const First& rhs) const                      { return lhs.first < rhs; }
            inline bool operator()(const First& lhs, const std::pair<First, Second>& rhs) const                      { return lhs < rhs.first; }
        };
    
    template <typename First, typename Second>
        class CompareSecond
        {
        public:
            inline bool operator()(std::pair<First, Second>& lhs, std::pair<First, Second>& rhs) const   { return lhs.second < rhs.second; }
            inline bool operator()(std::pair<First, Second>& lhs, Second& rhs) const                     { return lhs.second < rhs; }
            inline bool operator()(Second& lhs, std::pair<First, Second>& rhs) const                     { return lhs < rhs.second; }
        };

    template <typename First, typename Second, typename Allocator>
        static typename std::vector<std::pair<First, Second>, Allocator>::iterator LowerBound(
            typename std::vector<std::pair<First, Second>, Allocator>&v, First compareToFirst)
        {
            return std::lower_bound(v.begin(), v.end(), compareToFirst, CompareFirst<First, Second>());
        }

    template <typename First, typename Second, typename Allocator>
        static typename std::vector<std::pair<First, Second>, Allocator>::const_iterator LowerBound(
            typename const std::vector<std::pair<First, Second>, Allocator>&v, First compareToFirst)
        {
            return std::lower_bound(v.cbegin(), v.cend(), compareToFirst, CompareFirst<First, Second>());
        }

    template <typename Vector, typename Pred>
        static typename Vector::iterator FindIf(Vector& v, Pred&& predicate)
        {
            return std::find_if(v.begin(), v.end(), std::forward<Pred>(predicate));
        }

    template <typename Vector, typename Pred>
        static typename Vector::const_iterator FindIf(const Vector& v, Pred&& predicate)
        {
            return std::find_if(v.cbegin(), v.cend(), std::forward<Pred>(predicate));
        }

    template<typename SearchI, typename CompareI>
        SearchI FindFirstNotOf(
            SearchI searchStart, SearchI searchEnd,
            CompareI compareBegin, CompareI compareEnd)
        {
            auto i = searchStart;
            while (i != searchEnd && std::find(compareBegin, compareEnd, *i) != compareEnd)
                ++i;
            return i;
        }

    template<typename SearchI, typename CompareI>
        SearchI FindLastOf(
            SearchI searchStart, SearchI searchEnd,
            CompareI compareBegin, CompareI compareEnd)
        {
            auto i = searchEnd-1;
            while (i >= searchStart) {
                if (std::find(compareBegin, compareEnd, *i) != compareEnd)
                    return i;
                --i;
            }
            return searchEnd;
        }

    template<typename SearchI, typename CompareI>
        SearchI FindLastOf(
            SearchI searchStart, SearchI searchEnd,
            CompareI compare)
        {
            auto i = searchEnd-1;
            while (i >= searchStart) {
                if (*i == compare)
                    return i;
                --i;
            }
            return searchEnd;
        }

    template<typename Iterator>
        class IteratorRange : public std::pair<Iterator, Iterator>
        {
        public:
            Iterator begin() const      { return first; }
            Iterator end() const        { return second; }
            Iterator cbegin() const     { return first; }
            Iterator cend() const       { return second; }
            size_t size() const         { return std::distance(first, second); }
            bool empty() const          { return first == second; }

            decltype(*std::declval<Iterator>()) operator[](size_t index) const { return first[index]; }

            IteratorRange() : std::pair<Iterator, Iterator>(nullptr, nullptr) {}
            IteratorRange(Iterator f, Iterator s) : std::pair<Iterator, Iterator>(f, s) {}

            template<typename OtherIterator>
                IteratorRange(const std::pair<OtherIterator, OtherIterator>& copyFrom)
                    : std::pair<Iterator, Iterator>(copyFrom) {}

            IteratorRange(const std::initializer_list<typename std::decay<decltype(*std::declval<Iterator>())>::type>& copyFrom)
                : std::pair<Iterator, Iterator>(copyFrom.begin(), copyFrom.end()) {}

            template<typename OtherIterator>
                operator IteratorRange<OtherIterator>() const { return IteratorRange<OtherIterator>(cbegin(), cend()); }
        };

    template<typename Container>
        IteratorRange<const typename Container::value_type*> MakeIteratorRange(const Container& c)
        {
            return IteratorRange<const typename Container::value_type*>(AsPointer(c.cbegin()), AsPointer(c.cend()));
        }

    template<typename Container>
        IteratorRange<typename Container::value_type*> MakeIteratorRange(Container& c)
        {
            return IteratorRange<typename Container::value_type*>(AsPointer(c.begin()), AsPointer(c.end()));
        }
    
    template<typename Iterator>
        IteratorRange<Iterator> MakeIteratorRange(Iterator begin, Iterator end)
        {
            return IteratorRange<Iterator>(begin, end);
        }

    template<typename ArrayElement, int Count>
        IteratorRange<ArrayElement*> MakeIteratorRange(ArrayElement (&c)[Count])
        {
            return IteratorRange<ArrayElement*>(&c[0], &c[Count]);
        }

#pragma warning(push)
#pragma warning(disable:4789)       // buffer '' of size 12 bytes will be overrun; 4 bytes will be written starting at offset 12
    /// We can initialize from anything that looks like a collection of unsigned values
    /// This is a simple way to get casting from XLEMath::UInt2 (etc) types without
    /// having to include XLEMath headers from here.
    /// Plus, it will also work with any other types that expose a stl collection type
    /// interface.
    template<typename Type, unsigned Count> 
        class VectorPattern
    {
    public:
        Type _values[Count];

        VectorPattern(Type x=0, Type y=0, Type z=0, Type w=0)
        { 
            if (constant_expression<(Count > 0u)>::result()) _values[0] = x;
            if (constant_expression<(Count > 1u)>::result()) _values[1] = y;
            if (constant_expression<(Count > 2u)>::result()) _values[2] = z;
            if (constant_expression<(Count > 3u)>::result()) _values[3] = w;
        }

        template<int InitCount>
            VectorPattern(Type (&values)[InitCount])
        {
            for (unsigned c=0; c<Count; ++c) {
                if (c < InitCount) _values[c] = values[c];
                else _values[c] = (Type)0;
            }
        }

		Type operator[](unsigned index) const { return _values[index]; }
        Type& operator[](unsigned index) { return _values[index]; }

        template<
			typename Source,
			std::enable_if<std::is_assignable<Type, Source>::value>* = nullptr>
            VectorPattern(const Source& src)
            {
                auto size = std::size(src);
                unsigned c=0;
                for (; c<std::min(unsigned(size), Count); ++c) _values[c] = src[c];
                for (; c<Count; ++c) _values[c] = 0;
            }
    };
#pragma warning(pop)

}

// support for missing "std::size" in earlier versions of visual studio STL
#if (STL_ACTIVE == STL_MSVC) && (_MSC_VER <= 1800)
    namespace std
    {
        template<typename ValueType, size_t N>
            /*constexpr*/ size_t size(ValueType (&c)[N]) { return N; }
    }
#endif

using namespace Utility;
