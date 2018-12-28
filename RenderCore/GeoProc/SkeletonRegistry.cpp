// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SkeletonRegistry.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{

	std::string SkeletonRegistry::Skeleton::RegisterBindingName(NascentObjectGuid id, const std::string& defaultBindingName)
	{
		// We can't have multiple binding names overlapping. So, if we hit a duplicate name, we must make the binding
		// name unique somehow.
		// The trick is, we don't want our new unique name to overlap with some node name that might be registered 
		// later. Because some authoring tools will do similar kinds of things to encourage unique names, it's not
		// unlikely to end up overlapping some other name
		bool foundOverlappingName = false;
		for (const auto&existing:_importantNodes) {
			if (existing._id == id)
				return existing._bindingName;
			foundOverlappingName |= defaultBindingName == existing._bindingName;
		}

		std::string finalBindingName = defaultBindingName;
		if (foundOverlappingName) {
			unsigned uniqueIdx = 1;
			for (;; ++uniqueIdx) {
				finalBindingName = defaultBindingName + "[" + std::to_string(uniqueIdx) + "]";

				bool foundOverlappingName2 = false;
				for (const auto&existing:_importantNodes)
					foundOverlappingName2 |= finalBindingName == existing._bindingName;

				if (!foundOverlappingName2) break;
			}
		}
	
		_importantNodes.push_back({id, finalBindingName});
		return finalBindingName;
	}

	std::string SkeletonRegistry::Skeleton::GetBindingName(NascentObjectGuid id)
	{
		for (const auto&existing:_importantNodes)
			if (existing._id == id)
				return existing._bindingName;
		return {};
	}

	auto SkeletonRegistry::GetSkinningSkeleton(NascentObjectGuid skeletonRoot) -> Skeleton&
	{
		auto i = LowerBound(_skinningSkeletons, skeletonRoot);
		if (i == _skinningSkeletons.end() || !(i->first == skeletonRoot))
			i = _skinningSkeletons.insert(i, {skeletonRoot, {}});
		return i->second;
	}

	SkeletonRegistry::SkeletonRegistry()
	{}

    SkeletonRegistry::~SkeletonRegistry()
	{}

}}}

