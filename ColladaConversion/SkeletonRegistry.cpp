// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SkeletonRegistry.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore { namespace ColladaConversion
{

    bool SkeletonRegistry::IsImportant(ObjectGuid node) const
    {
        auto i = LowerBound(_outputMatrixIndicies, node);
        return i != _outputMatrixIndicies.end() && i->first == node;
    }

    const Float4x4* SkeletonRegistry::GetInverseBindMatrix(ObjectGuid node) const
    {
        auto i = LowerBound(_inverseBindMatrics, node);
        if (i != _inverseBindMatrics.end() && i->first == node)
            return &i->second;
        return nullptr;
    }

    unsigned SkeletonRegistry::GetOutputMatrixIndex(ObjectGuid node)
    {
        auto i = LowerBound(_outputMatrixIndicies, node);
        if (i != _outputMatrixIndicies.end() && i->first == node)
            return i->second;
        auto result = _nextOutputIndex++;
        _outputMatrixIndicies.insert(i, std::make_pair(node, result));
        return result;
    }

    std::string SkeletonRegistry::GetBindingName(ObjectGuid node) const
    {
        auto i = LowerBound(_outputMatrixBindingNames, node);
        if (i != _outputMatrixBindingNames.end() && i->first == node)
            return i->second;
        return std::string();
    }

    bool SkeletonRegistry::TryRegisterNode(ObjectGuid node, const char bindingName[])
    {
        auto i = LowerBound(_outputMatrixBindingNames, node);
        if (i != _outputMatrixBindingNames.end() && i->first == node)
            return i->second == bindingName;    // return true only if it matches the previously registered name

        // look for other nodes bound to the same name
        for(const auto&i:_outputMatrixBindingNames)
            if (i.second == bindingName)
                return false;   // duplicate binding name applied to some other node

        _outputMatrixBindingNames.insert(i, std::make_pair(node, bindingName));
        return true;
    }

    void SkeletonRegistry::AttachInverseBindMatrix(ObjectGuid node, const Float4x4& inverseBind)
    {
        auto i = LowerBound(_inverseBindMatrics, node);
        if (i != _inverseBindMatrics.end() && i->first == node) {
            i->second = inverseBind;
        } else {
            _inverseBindMatrics.insert(i, std::make_pair(node, inverseBind));
        }
    }

    void    SkeletonRegistry::MarkParameterAnimated(const std::string& paramName)
    {
        auto i = std::lower_bound(_markParameterAnimated.cbegin(), _markParameterAnimated.cend(), paramName);
        if (i != _markParameterAnimated.cend() && (*i) == paramName) return;
        _markParameterAnimated.insert(i, paramName);
    }

    bool    SkeletonRegistry::IsAnimated(const std::string& paramName) const
    {
        auto i = std::lower_bound(_markParameterAnimated.cbegin(), _markParameterAnimated.cend(), paramName);
        return i != _markParameterAnimated.cend() && (*i) == paramName;
    }

    SkeletonRegistry::SkeletonRegistry() : _nextOutputIndex(0) {}
    SkeletonRegistry::~SkeletonRegistry() {}
}}
