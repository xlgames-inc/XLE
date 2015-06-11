// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TableOfObjects.h"
#include "Scaffold.h"
#include "ModelCommandStream.h"
#include "STransformationMachine.h"
#include "../Utility/Conversion.h"
#include "../Utility/MemoryUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static std::string SkeletonBindingName(const Node& node);
    static ObjectGuid AsObjectGuid(const Node& node);

    static bool IsUseful(const Node& node, const TransformReferences& skeletonReferences);

    void PushNode(
        NascentSkeleton& skeleton,
        const Node& node,
        const TableOfObjects& accessableObjects,
        const TransformReferences& skeletonReferences)
    {
        using namespace COLLADAFW;

        if (!IsUseful(node, skeletonReferences)) return;

        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.GetFirstTransform(), SkeletonBindingName(node).c_str());

            //
            //      We have to assume we need an output matrix. We don't really know
            //      which nodes need output matrices at this point (because we haven't 
            //      got all the downstream skinning data). So, let's just assume it's needed.
            //
        auto nodeId = AsObjectGuid(node);
        bool isReferenced = skeletonReferences.HasNode(nodeId);

            // DavidJ -- hack! -- When writing a "skeleton" we need to include all nodes, even those that aren't
            //              referenced within the same file. This is because the node might become an output-interface
            //              node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //              output interface transforms / bones... Or maybe we could just include everything when
            //              compiling a skeleton...?
        isReferenced = true;
        if (isReferenced) {
            unsigned thisOutputMatrix = skeleton.GetTransformationMachine().GetOutputMatrixMarker();

                //
                //      (We can't instantiate the skin controllers yet, because we can't be sure
                //          we've already parsed the skeleton nodes)
                //      But we can write a tag to find the output matrix later
                //          (we also need a tag for all nodes with instance controllers in them)
                //

            auto* inverseBind = skeletonReferences.GetInverseBindMatrix(nodeId);
            if (inverseBind) {
                    // note -- there may be problems here, because the "name" of the node isn't necessarily
                    //          unique. There are unique ids in collada, however. We some some unique identifier
                    //          can can be seen in Max, and can be used to associate different files with shared
                    //          references (eg, animations, skeletons and skins in separate files)
                skeleton.GetTransformationMachine().RegisterJointName(
                    SkeletonBindingName(node), 
                    *inverseBind, thisOutputMatrix);
            }
        }

        auto child = node.GetFirstChild();
        while (child) {
            PushNode(skeleton, child, accessableObjects, skeletonReferences);
            child = child.GetNextSibling();
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
    }


    static std::string SkeletonBindingName(const Node& node)
    {
        return Conversion::Convert<std::string>(
            std::basic_string<utf8>(node.GetName()._start, node.GetName()._end));
    }

    static ObjectGuid AsObjectGuid(const Node& node)
    {
        return ObjectGuid(Hash64(node.GetId()._start, node.GetId()._end));
    }

    static bool IsUseful(const Node& node, const TransformReferences& skeletonReferences)
    {
        if (skeletonReferences.HasNode(AsObjectGuid(node))) return true;
        auto child = node.GetFirstChild();
        while (child) {
            if (IsUseful(child, skeletonReferences)) return true;
            child = child.GetNextSibling();
        }
        return false;
    }

}}

