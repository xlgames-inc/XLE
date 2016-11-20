// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SkeletonRegistry.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class CompareId
    {
    public:
        bool operator()(const SkeletonRegistry::ImportantNode& lhs, NascentObjectGuid rhs) const   { return lhs._id < rhs; }
        bool operator()(NascentObjectGuid lhs, const SkeletonRegistry::ImportantNode& rhs) const   { return lhs < rhs._id; }
        bool operator()(NascentObjectGuid lhs, NascentObjectGuid rhs) const                               { return lhs < rhs; }
    };

    bool SkeletonRegistry::IsImportant(NascentObjectGuid node) const
    {
        if (node == NascentObjectGuid()) return false;
        auto i = std::lower_bound(_importantNodes.begin(), _importantNodes.end(), node, CompareId());
        return i != _importantNodes.end() && i->_id == node;
    }

    auto SkeletonRegistry::GetOutputMatrixIndex(NascentObjectGuid node) -> TransformMarker
    {
        if (node == NascentObjectGuid()) return ~0u;
        auto i = std::lower_bound(_importantNodes.begin(), _importantNodes.end(), node, CompareId());
        if (i != _importantNodes.end() && i->_id == node) {
            if (i->_transformMarker == TransformMarker_UnSet)
                i->_transformMarker = _nextOutputIndex++;
            return i->_transformMarker;
        }

        auto result = _nextOutputIndex++;
        _importantNodes.insert(i, ImportantNode(node, std::string(), result, Identity<Float4x4>(), false));
        return result;
    }

    bool SkeletonRegistry::TryRegisterNode(NascentObjectGuid node, const char bindingName[])
    {
        if (node == NascentObjectGuid()) return false;
        // look for other nodes bound to the same name
        for(const auto&i:_importantNodes)
            if (i._bindingName == bindingName)
                return i._id == node;   // duplicate binding name applied to some other node

        auto i = std::lower_bound(_importantNodes.begin(), _importantNodes.end(), node, CompareId());
        if (i != _importantNodes.end() && i->_id == node) {
            if (i->_bindingName.empty()) {
                i->_bindingName = bindingName;
                return true;
            }
            return i->_bindingName == bindingName;    // return true only if it matches the previously registered name
        }

        _importantNodes.insert(i, ImportantNode(node, bindingName, TransformMarker_UnSet, Identity<Float4x4>(), false));
        return true;
    }

    void SkeletonRegistry::AttachInverseBindMatrix(NascentObjectGuid node, const Float4x4& inverseBind)
    {
        if (node == NascentObjectGuid()) return;
        auto i = std::lower_bound(_importantNodes.begin(), _importantNodes.end(), node, CompareId());
        if (i != _importantNodes.end() && i->_id == node) {
            i->_inverseBind = inverseBind;
            i->_hasInverseBind = true;
        } else {
            _importantNodes.insert(i, ImportantNode(node, std::string(), TransformMarker_UnSet, inverseBind, true));
        }
    }

    auto    SkeletonRegistry::GetImportantNodes() const -> IteratorRange<const ImportantNode*>
    {
        return MakeIteratorRange(_importantNodes);
    }

    auto SkeletonRegistry::GetNode(NascentObjectGuid node) const -> ImportantNode
    {
        auto i = std::lower_bound(_importantNodes.begin(), _importantNodes.end(), node, CompareId());
        if (i != _importantNodes.end() && i->_id == node)
            return *i;
        return ImportantNode();
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

}}}

