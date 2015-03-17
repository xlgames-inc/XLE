// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

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
}

using namespace Utility;


