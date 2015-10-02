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
            Iterator begin()        { return first; }
            Iterator end()          { return second; }
            Iterator cbegin() const { return first; }
            Iterator cend() const   { return second; }
            size_t size() const     { return std::distance(first, second); }
            bool empty() const      { return first == second; }

            decltype(*std::declval<Iterator>()) operator[](size_t index) const { return first[index]; }

            IteratorRange() : std::pair<Iterator, Iterator>(nullptr, nullptr) {}
            IteratorRange(Iterator f, Iterator s) : std::pair<Iterator, Iterator>(f, s) {}

            template<typename OtherIterator>
                IteratorRange(const std::pair<OtherIterator, OtherIterator>& copyFrom)
                    : std::pair<Iterator, Iterator>(copyFrom) {}

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
}

using namespace Utility;


