// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ConversionCore.h"
#include "../Math/Matrix.h"
#include <vector>
#include <algorithm>

namespace RenderCore { namespace ColladaConversion
{
    class SkeletonRegistry
    {
    public:
        bool            IsImportant(ObjectGuid node) const;

        bool            TryRegisterNode(ObjectGuid node, const char bindingName[]);
        unsigned        GetOutputMatrixIndex(ObjectGuid node);
        std::string     GetBindingName(ObjectGuid node) const;

        const Float4x4* GetInverseBindMatrix(ObjectGuid node) const;
        void            AttachInverseBindMatrix(ObjectGuid node, const Float4x4& inverseBind);

        void            MarkParameterAnimated(const std::string& paramName);
        bool            IsAnimated(const std::string& paramName) const;

        SkeletonRegistry();
        ~SkeletonRegistry();
    protected:
        using OutputMatrixIndex = unsigned;
        static const OutputMatrixIndex OutputMatrixIndex_UnSet = ~unsigned(0);

        std::vector<std::pair<ObjectGuid, std::string>> _outputMatrixBindingNames;
        std::vector<std::pair<ObjectGuid, OutputMatrixIndex>> _outputMatrixIndicies;
        std::vector<std::pair<ObjectGuid, Float4x4>> _inverseBindMatrics;
        std::vector<std::string> _markParameterAnimated;

        OutputMatrixIndex _nextOutputIndex;
    };
}}

