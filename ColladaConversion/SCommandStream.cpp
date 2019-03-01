// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SCommandStream.h"
#include "STransformationMachine.h"
#include "SRawGeometry.h"
#include "SAnimation.h"

#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"

#include "../RenderCore/GeoProc/NascentCommandStream.h"
#include "../RenderCore/GeoProc/NascentRawGeometry.h"
#include "../RenderCore/GeoProc/NascentAnimController.h"
#include "../RenderCore/GeoProc/GeoProcUtil.h"

#include "../RenderCore/Assets/MaterialScaffold.h"  // for MakeMaterialGuid
#include "../RenderCore/Format.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include <string>

namespace ColladaConversion
{
    static NascentObjectGuid   AsObjectGuid(const Node& node);

///////////////////////////////////////////////////////////////////////////////////////////////////

    LODDesc GetLevelOfDetail(const ::ColladaConversion::Node& node)
    {
        // We're going assign a level of detail to this node based on naming conventions. We'll
        // look at the name of the node (rather than the name of the geometry object) and look
        // for an indicator that it includes a LOD number.
        // We're looking for something like "$lod" or "_lod". This should be followed by an integer,
        // and with an underscore following.
        if (    XlBeginsWithI(node.GetName(), MakeStringSection(u("_lod")))
            ||  XlBeginsWithI(node.GetName(), MakeStringSection(u("$lod")))) {

            auto nextSection = MakeStringSection(node.GetName().begin()+4, node.GetName().end());
            uint32 lod = 0;
            auto* parseEnd = FastParseElement(lod, nextSection.begin(), nextSection.end());
            if (parseEnd < nextSection.end() && *parseEnd == '_')
                return LODDesc { lod, true, MakeStringSection(parseEnd+1, node.GetName().end()) };
            Log(Warning) << "Node name (" << Conversion::Convert<std::string>(node.GetName().AsString()) << ") looks like it contains a lod index, but parse failed. Defaulting to lod 0." << std::endl;
        }
        return LODDesc { 0, false, StringSection<utf8>() };
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	void BuildSkeleton(NascentSkeleton& skeleton, const Node& node, StringSection<> skeletonName)
    {
        auto nodeId = AsObjectGuid(node);
        auto bindingName = SkeletonBindingName(node);

		auto pushCount = PushTransformations(
			skeleton.GetSkeletonMachine(),
			skeleton.GetInterface(),
			skeleton.GetDefaultParameters(),
			node.GetFirstTransform(), bindingName.c_str(),
			[](StringSection<>) { return true; });

		uint32 outputMarker = ~0u;
		if (skeleton.GetInterface().TryRegisterJointName(outputMarker, skeletonName, bindingName)) {
			skeleton.GetSkeletonMachine().WriteOutputMarker(outputMarker);
		} else {
			Throw(::Exceptions::BasicLabel("Couldn't register joint name in skeleton interface for node (%s:%s)", skeletonName.AsString().c_str(), bindingName.c_str()));
		}

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
			BuildSkeleton(skeleton, child, skeletonName);
            child = child.GetNextSibling();
        }

        skeleton.GetSkeletonMachine().Pop(pushCount);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	auto BuildMaterialTableStrings(
        IteratorRange<const InstanceGeometry::MaterialBinding*> bindings,
        const std::vector<uint64>& rawGeoBindingSymbols,
        const URIResolveContext& resolveContext)

        -> std::vector<std::string>
    {

            //
            //  For each material referenced in the raw geometry, try to 
            //  match it with a material we've built during collada processing
            //      We have to map it via the binding table in the InstanceGeometry
            //
                        
        std::vector<std::string> materialGuids;
        materialGuids.resize(rawGeoBindingSymbols.size());

        for (auto b=bindings.begin(); b<bindings.end(); ++b) {
            auto hashedSymbol = Hash64(b->_bindingSymbol._start, b->_bindingSymbol._end);

            for (auto i=rawGeoBindingSymbols.cbegin(); i!=rawGeoBindingSymbols.cend(); ++i) {
                if (*i != hashedSymbol) continue;
            
                auto index = std::distance(rawGeoBindingSymbols.cbegin(), i);

                std::string newMaterialGuid;

                GuidReference ref(b->_reference);
                auto* file = resolveContext.FindFile(ref._fileHash);
                if (file) {
                    const auto* mat = file->FindMaterial(ref._id);
                    if (mat) {
                        newMaterialGuid = mat->_name.Cast<char>().AsString();
                    }
                }


                if (!materialGuids[index].empty() && materialGuids[index] != newMaterialGuid) {

                        // Some collada files can actually have multiple instance_material elements for
                        // the same binding symbol. Let's throw an exception in this case (but only
                        // if the bindings don't agree)
                    Throw(::Exceptions::BasicLabel("Single material binding symbol is bound to multiple different materials in geometry instantiation"));
                }

                materialGuids[index] = newMaterialGuid;
            }
        }

        return std::move(materialGuids);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::string SkeletonBindingName(const Node& node)
    {
        // Both "name" and "id" are optional.
		// It turns out we must priortize the name here, because of cross-file binding.
		// There's no real guarantee that 2 nodes will have the same ids in different files,
		// since the ids are algorithmically generated from the names.
		// However if we keep to a convention of not duplicating names, we 
        if (!node.GetName().IsEmpty())
            return ColladaConversion::AsString(node.GetName()); 
		if (!node.GetId().IsEmpty())
            return ColladaConversion::AsString(node.GetId().GetOriginal());
        return XlDynFormatString("Unnamed%i", (unsigned)node.GetIndex());
    }

    static NascentObjectGuid AsObjectGuid(const Node& node)
    { 
        if (!node.GetId().IsEmpty())
            return node.GetId().GetHash(); 
        if (!node.GetName().IsEmpty())
            return Hash64(node.GetName().begin(), node.GetName().end());

        // If we have no name & no id -- it is truly anonymous. 
        // We can just use the index of the node, it's the only unique
        // thing we have.
        return NascentObjectGuid(node.GetIndex());
    }

}

