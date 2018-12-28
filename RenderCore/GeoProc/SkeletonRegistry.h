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
		class ImportantNode
        {
        public:
            NascentObjectGuid   _id;
            std::string			_bindingName;
        };

		class Skeleton
		{
		public:
			NascentObjectGuid _rootNode;
			std::vector<ImportantNode> _importantNodes;
			std::vector<std::string> _markParameterAnimated;

			std::string RegisterBindingName(NascentObjectGuid id, const std::string& defaultBindingName);
			std::string GetBindingName(NascentObjectGuid id);
		};

		Skeleton& GetBasicStructure() { return _basicStructure; }
		Skeleton& GetSkinningSkeleton(NascentObjectGuid skeletonRoot);

		IteratorRange<const std::pair<NascentObjectGuid, Skeleton>*> GetSkinningSkeletons() const { return MakeIteratorRange(_skinningSkeletons); }

        SkeletonRegistry();
        ~SkeletonRegistry();
    protected:
		Skeleton _basicStructure;
		std::vector<std::pair<NascentObjectGuid, Skeleton>> _skinningSkeletons;
    };
}}}

