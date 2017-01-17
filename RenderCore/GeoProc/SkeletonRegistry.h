// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentObjectGuid.h"
#include "../Math/Matrix.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <algorithm>

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class SkeletonRegistry
    {
    public:
        bool            IsImportant(NascentObjectGuid node) const;

        class ImportantNode
        {
        public:
            NascentObjectGuid   _id;
            std::string			_bindingName;
        };

        auto            GetImportantNodes() const -> IteratorRange<const ImportantNode*>;
        ImportantNode   GetNode(NascentObjectGuid node) const;
        
        bool            TryRegisterNode(NascentObjectGuid node, const char bindingName[]);
		std::string		GetBindingName(NascentObjectGuid node) const;

        void            MarkParameterAnimated(const std::string& paramName);
        bool            IsAnimated(const std::string& paramName) const;

        SkeletonRegistry();
        ~SkeletonRegistry();
    protected:
        std::vector<ImportantNode> _importantNodes;
        std::vector<std::string> _markParameterAnimated;
    };
}}}

