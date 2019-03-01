// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"    // for AsString
#include "ConversionConfig.h"

#include "SEffect.h"
#include "SCommandStream.h"
#include "SAnimation.h"
#include "SCommandStream.h"
#include "SRawGeometry.h"

#include "../RenderCore/GeoProc/NascentCommandStream.h"
#include "../RenderCore/GeoProc/NascentRawGeometry.h"
#include "../RenderCore/GeoProc/NascentAnimController.h"
#include "../RenderCore/GeoProc/NascentObjectsSerialize.h"
#include "../RenderCore/GeoProc/NascentModel.h"
#include "../RenderCore/GeoProc/MeshDatabase.h"

#include "../RenderCore/Assets/ModelImmutableData.h"      // just for RenderCore::Assets::SkeletonBinding
#include "../RenderCore/Assets/AssetUtils.h"
#include "../RenderCore/Assets/RawMaterial.h"
#include "../Assets/ICompileOperation.h"
#include "../Assets/IFileSystem.h"

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileSystemMonitor.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringFormat.h"
#include "../ConsoleRig/OutputStream.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/Log.h"
#include <memory>
#include <map>
#include <set>

#pragma warning(disable:4505) // unreferenced local function has been removed

namespace ColladaConversion
{

	static const auto ChunkType_Text = ConstHash64<'Text'>::Value;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ColladaCompileOp : public ::Assets::ICompileOperation
    {
    public:
        std::string _name;
		::Assets::rstring _rootNode;
		ImportConfiguration _cfg;
        MemoryMappedFile _fileData;
        std::shared_ptr<DocumentScaffold> _doc;
        ::ColladaConversion::URIResolveContext _resolveContext;
		std::vector<TargetDesc> _targets;
		std::vector<::Assets::DependentFileState> _dependencies;

		std::vector<TargetDesc> GetTargets() const;
		std::vector<OperationResult> SerializeTarget(unsigned idx);
		std::vector<::Assets::DependentFileState> GetDependencies() const;

		ColladaCompileOp();
		~ColladaCompileOp();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Float4x4 BuildCoordinateTransform(const AssetDesc& assetDesc)
    {
        // When the "up" vector, or the units specified in the collada header
        // don't match our expectations, we can apply an extra transform.
        // This should transform some given input coordinates into our expected
        // final output.
        // Here, we will convert to 1.f unit == 1.f meter, with +Z being up.
        float scale = assetDesc._metersPerUnit;
        Float3x3 axisTransform;
        switch (assetDesc._upAxis) {
        case AssetDesc::UpAxis::X:
                // -Y --> +X
                // +Z --> -Y
                // +X --> +Z
            axisTransform = Float3x3(
                 0.f, -1.f,  0.f,
                 0.f,  0.f, -1.f,
                 1.f,  0.f,  0.f);
            break;

        case AssetDesc::UpAxis::Y:
                // +X --> +X
                // +Z --> -Y
                // +Y --> +Z
                //  hmm... winding flip...?
            axisTransform = Float3x3(
                 1.f,  0.f,   0.f,
                 0.f,  0.f,  -1.f,
                 0.f,  1.f,   0.f);
            break;

        default:
        case AssetDesc::UpAxis::Z:
            axisTransform = Identity<Float3x3>();
            break;
        }

        return AsFloat4x4(Float3x3(scale * axisTransform));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static NascentObjectGuid ConvertGeometryBlock(
		NascentModel& model,
		std::map<NascentObjectGuid, std::vector<uint64_t>>& geoBlockMatBindings,
		Section reference,
		const ::ColladaConversion::URIResolveContext& resolveContext,
		const ImportConfiguration& cfg)
	{
		GuidReference refGuid(reference);

		auto* scaffoldGeo = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
		if (!scaffoldGeo) {
				// look for a skin controller instead... We will use the geometry object that is referenced
				// by the controller
			auto* scaffoldController = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindSkinController);
			refGuid = GuidReference{scaffoldController->GetBaseMesh()};
			scaffoldGeo = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
		}

		if (!scaffoldGeo)
			Throw(::Exceptions::BasicLabel("Could not found geometry object to instantiate (%s)",
				reference.AsString().c_str()));

		NascentObjectGuid geoId(refGuid._id, refGuid._fileHash);
		auto* existingGeometry = model.FindGeometryBlock(geoId);
		if (!existingGeometry) {
			auto convertedMesh = Convert(*scaffoldGeo, resolveContext, cfg);
			if (convertedMesh._geoBlock._drawCalls.empty()) {
                    
					// everything else should be empty as well...
				assert(!convertedMesh._geoBlock._mesh || convertedMesh._geoBlock._mesh->GetUnifiedVertexCount() == 0);
				assert(convertedMesh._geoBlock._indices.empty());
				assert(convertedMesh._geoBlock._meshVertexIndexToSrcIndex.empty());
                
				Throw(::Exceptions::BasicLabel(
					"Geometry object is empty (%s)", reference.AsString().c_str()));
			}

			model.Add(geoId, scaffoldGeo->GetName().Cast<char>().AsString(), std::move(convertedMesh._geoBlock));
			geoBlockMatBindings.insert(std::make_pair(geoId, std::move(convertedMesh._matBindingSymbols)));
		}

		return geoId;
	}

	static void ConvertCommand(
		NascentModel& model,
		std::map<NascentObjectGuid, std::vector<uint64_t>>& geoBlockMatBindings,
		const Node& attachedNode,
		NascentObjectGuid geoId, NascentObjectGuid controllerId,
		const std::string& localToModelBinding,
		IteratorRange<const InstanceGeometry::MaterialBinding*> materialBindings,
		const ::ColladaConversion::URIResolveContext& resolveContext)
	{
		auto mati = geoBlockMatBindings.find(geoId);
		assert(mati != geoBlockMatBindings.end());

		auto materials = BuildMaterialTableStrings(
			materialBindings,
			mati->second, resolveContext);

		model.Add(
			attachedNode.GetId().GetHash(),
			attachedNode.GetName().Cast<char>().AsString(),
			NascentModel::Command {
				geoId, controllerId,
				localToModelBinding,
				materials, 
				0
			});
	}

	static bool IsAncestorOf(const Node& node, IteratorRange<const Node*> roots) 
	{
		if (std::find(roots.begin(), roots.end(), node))
			return true;

		if (!node.GetParent())
			return false;

		return IsAncestorOf(node.GetParent(), roots);
	}

	static std::string GetSkeletonName(const InstanceController& instController)
	{
		auto skeleName = instController.GetSkeleton().Cast<char>().AsString();
		if (!skeleName.empty() && *skeleName.begin() == '#')
			skeleName.erase(skeleName.begin());
		return skeleName;
	}

	static NascentModel ConvertModel(const ColladaCompileOp& input, const VisualScene& scene, IteratorRange<const Node*> roots)
	{
		NascentModel model;
		std::map<NascentObjectGuid, std::vector<uint64_t>> geoBlockMatBindings;

		///////////////////
		for (unsigned instGeoIndex=0; instGeoIndex<scene.GetInstanceGeometryCount(); ++instGeoIndex) {
            const auto& instGeo = scene.GetInstanceGeometry(instGeoIndex);
			auto attachNode = scene.GetInstanceGeometry_Attach(instGeoIndex);
			if (!IsAncestorOf(attachNode, roots))
				continue;

			TRY {
				auto geoId = ConvertGeometryBlock(
					model, geoBlockMatBindings,
					instGeo._reference,
					input._resolveContext, input._cfg);

				ConvertCommand(
					model, geoBlockMatBindings,
					attachNode,
					geoId, {},
					SkeletonBindingName(attachNode), MakeIteratorRange(instGeo._matBindings),
					input._resolveContext);
            } CATCH(const std::exception& e) {
                Log(Warning) << "Got exception while instantiating geometry (" << instGeo._reference.AsString().c_str() << "). Exception details:" << std::endl;
                Log(Warning) << e.what() << std::endl;
            } CATCH(...) {
                Log(Warning) << "Got unknown exception while instantiating geometry (" << instGeo._reference.AsString().c_str() << ")." << std::endl;
            } CATCH_END
        }

		///////////////////
		for (unsigned instSkinControllerIndex=0; instSkinControllerIndex<scene.GetInstanceControllerCount(); ++instSkinControllerIndex) {
            const auto& instController = scene.GetInstanceController(instSkinControllerIndex);
			auto attachNode = scene.GetInstanceController_Attach(instSkinControllerIndex);
			if (!IsAncestorOf(attachNode, roots))
				continue;

			NascentObjectGuid geoId;

			bool skinSuccessful = false;
            TRY {
				geoId = ConvertGeometryBlock(
					model, geoBlockMatBindings,
					instController._reference,
					input._resolveContext, input._cfg);

				//////////////////

				GuidReference controllerRef(instController._reference);
				NascentObjectGuid controllerId(controllerRef._id, controllerRef._fileHash);
				auto* scaffoldController = FindElement(controllerRef, input._resolveContext, &IDocScopeIdResolver::FindSkinController);
				if (!scaffoldController)
					Throw(::Exceptions::BasicLabel("Could not find controller object to instantiate (%s)",
						instController._reference.AsString().c_str()));

				auto controller = Convert(*scaffoldController, input._resolveContext, input._cfg);
				auto skeleName = GetSkeletonName(instController);

				model.Add(
					controllerId, 
					scaffoldController->GetName().Cast<char>().AsString(),
					NascentModel::SkinControllerBlock {
						std::make_shared<UnboundSkinController>(std::move(controller)),
						skeleName
					});

				ConvertCommand(
					model, geoBlockMatBindings,
					attachNode,
					geoId, controllerId,
					SkeletonBindingName(attachNode), MakeIteratorRange(instController._matBindings),
					input._resolveContext);

                skinSuccessful = true;
            } CATCH(const std::exception& e) {
                Log(Warning) << "Got exception while instantiating controller (" << instController._reference.AsString().c_str() << "). Exception details:" << std::endl;
                Log(Warning) << e.what() << std::endl;
            } CATCH(...) {
                Log(Warning) << "Got unknown exception while instantiating controller (" << instController._reference.AsString().c_str() << ")." << std::endl;
            } CATCH_END

            if (!skinSuccessful) {
                    // if we failed to instantiate this object as a skinned controller,
                    // we can try to fall back to a static geometry object. This fallback
                    // can be required for some controller objects that use rigid animation
                    //  -- they can have a skin controller with no joints (meaning at the 
                    //      only transform that can affect them is the parent node -- or maybe the skeleton root?)
                Log(Warning) << "Could not instantiate controller as a skinned object. Falling back to rigid object." << std::endl;
                TRY {
					ConvertCommand(
						model, geoBlockMatBindings,
						attachNode,
						geoId, {},
						SkeletonBindingName(attachNode), MakeIteratorRange(instController._matBindings),
						input._resolveContext);
                } CATCH(const std::exception& e) {
                    Log(Warning) << "Got exception while instantiating geometry (after controller failed) (" << instController._reference.AsString().c_str() << "). Exception details:" << std::endl;
                    Log(Warning) << e.what() << std::endl;
                } CATCH(...) {
                    Log(Warning) << "Got unknown exception while instantiating geometry (after controller failed) (" << instController._reference.AsString().c_str() << ")." << std::endl;
                } CATCH_END
            }
        }

		return model;
	}

	static void OptimizeSkeleton(NascentSkeleton& embeddedSkeleton, NascentModel& model)
	{
		{
			auto filteringSkeleInterface = model.BuildSkeletonInterface();
			filteringSkeleInterface.insert(filteringSkeleInterface.begin(), std::make_pair(std::string{}, "identity"));
			embeddedSkeleton.FilterOutputInterface(MakeIteratorRange(filteringSkeleInterface));
		}

		{
			auto finalSkeleInterface = embeddedSkeleton.GetInterface().GetOutputInterface();
			ModelTransMachineOptimizer optimizer(model, finalSkeleInterface);
			embeddedSkeleton.GetSkeletonMachine().Optimize(optimizer);

			for (unsigned c=0; c<finalSkeleInterface.size(); ++c) {
				const auto& mat = optimizer.GetMergedOutputMatrices()[c];
				if (!Equivalent(mat, Identity<Float4x4>(), 1e-3f)) {
					assert(finalSkeleInterface[c].first.empty());	// this operation only makes sense for the basic structure skeleton
					model.ApplyTransform(finalSkeleInterface[c].second, mat);
				}
			}
		}
	}

	static std::set<std::string> GetSkeletons(NascentModel& model)
	{
		std::set<std::string> skinningSkeletons;
		for (const auto& skinController:model.GetSkinControllerBlocks())
			skinningSkeletons.insert(skinController.second._skeleton);
		skinningSkeletons.insert(std::string{});
		return skinningSkeletons;
	}

	static NascentSkeleton ConvertSkeleton(
		const ColladaCompileOp& input, const VisualScene& scene, const std::set<std::string>& skinningSkeletons, IteratorRange<const Node*> roots)
	{
		NascentSkeleton result;
		{
			uint32 outputMarker = ~0u;
			if (result.GetInterface().TryRegisterJointName(outputMarker, "", "identity"))
				result.GetSkeletonMachine().WriteOutputMarker(outputMarker);
		}

		for (const auto&skeletonName:skinningSkeletons) {
			if (skeletonName.empty()) {
				unsigned topLevelPops = 0;
				auto coordinateTransform = BuildCoordinateTransform(input._doc->GetAssetDesc());
				if (!Equivalent(coordinateTransform, Identity<Float4x4>(), 1e-5f)) {
						// Push on the coordinate transform (if there is one)
						// This should be optimised into other matrices (or even into
						// the geometry) when we perform the skeleton optimisation steps.
					topLevelPops = result.GetSkeletonMachine().PushTransformation(
						coordinateTransform);
				}

				for (const auto&root:roots)
					BuildSkeleton(result, root);
				result.GetSkeletonMachine().Pop(topLevelPops);
			} else {
				auto node = scene.GetRootNode().FindBreadthFirst(
					[skeletonName](const Node& node) {
						return skeletonName == SkeletonBindingName(node);
					});
				if (!node)
					Throw(::Exceptions::BasicLabel("Could not find node for skeleton with binding name (%s)", skeletonName.c_str()));

				// Note that we include this skeleton, even if it isn't strictly an ancestor of the nodes
				// in roots. This is so skin controllers can reference skeletons in arbitrary parts of the scene
				BuildSkeleton(result, node, skeletonName);
			}
		}
		
		return result;
	}

	static std::vector<Node> FindRoots(const VisualScene& scene, StringSection<utf8> rootNodeName)
	{
		std::vector<Node> roots;
		if (rootNodeName.IsEmpty()) {
			roots.push_back(scene.GetRootNode());
		} else {
			roots = scene.GetRootNode().FindAllBreadthFirst(
				[rootNodeName](const Node& n)
				{
					if (XlEqString(n.GetName(), rootNodeName)) return true;
					auto desc = GetLevelOfDetail(n);
					return desc._isLODRoot && XlEqString(desc._remainingName, rootNodeName);
				});
		}
		return roots;
	}

	class PreparedSkinFile
    {
    public:
		std::vector<::Assets::ICompileOperation::OperationResult> _serializedResult;

        PreparedSkinFile(const ColladaCompileOp&, const VisualScene&, StringSection<utf8>);
    };

	PreparedSkinFile::PreparedSkinFile(const ColladaCompileOp& input, const VisualScene& scene, StringSection<utf8> rootNodeName)
    {
		auto roots = FindRoots(scene, rootNodeName);
		if (roots.empty()) return;

		auto model = ConvertModel(input, scene, MakeIteratorRange(roots));
		auto embeddedSkeleton = ConvertSkeleton(input, scene, GetSkeletons(model), MakeIteratorRange(roots));
		OptimizeSkeleton(embeddedSkeleton, model);

		_serializedResult = model.SerializeToChunks("model", embeddedSkeleton);
    }

    std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkin(const ColladaCompileOp& model, StringSection<utf8> rootNodeName)
    {
        const auto* scene = model._doc->FindVisualScene(
            GuidReference(model._doc->_visualScene)._id);
        if (!scene)
            Throw(::Exceptions::BasicLabel("No visual scene found"));

        PreparedSkinFile skinFile(model, *scene, rootNodeName);
		return skinFile._serializedResult;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedSkeletonFile
    {
    public:
        NascentSkeleton _skeleton;

        PreparedSkeletonFile(const ColladaCompileOp&, const VisualScene& scene, StringSection<utf8> rootNodeName);
    };

    PreparedSkeletonFile::PreparedSkeletonFile(const ColladaCompileOp& input, const VisualScene& scene, StringSection<utf8> rootNodeName)
    {
		auto roots = FindRoots(scene, rootNodeName);
		if (roots.empty()) return;

		std::set<std::string> skinningSkeletons;
		for (unsigned instSkinControllerIndex=0; instSkinControllerIndex<scene.GetInstanceControllerCount(); ++instSkinControllerIndex) {
            const auto& instController = scene.GetInstanceController(instSkinControllerIndex);
			auto attachNode = scene.GetInstanceController_Attach(instSkinControllerIndex);
			if (!IsAncestorOf(attachNode, MakeIteratorRange(roots)))
				continue;

			skinningSkeletons.insert(GetSkeletonName(instController));
		}
		skinningSkeletons.insert(std::string{});

		_skeleton = ConvertSkeleton(input, scene, skinningSkeletons, MakeIteratorRange(roots));
        RenderCore::Assets::TransformationMachineOptimizer_Null optimizer;
        _skeleton.GetSkeletonMachine().Optimize(optimizer);
    }

    std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeleton(const ColladaCompileOp& input, StringSection<utf8> rootNodeName)
    {
		const auto* scene = input._doc->FindVisualScene(
            GuidReference(input._doc->_visualScene)._id);
        if (!scene)
            Throw(::Exceptions::BasicLabel("No visual scene found"));

        PreparedSkeletonFile skeleFile(input, *scene, rootNodeName);
        return SerializeSkeletonToChunks("skeleton", skeleFile._skeleton);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void SerializeMatTable(OutputStream& stream, const ColladaCompileOp& model)
    {
        OutputStreamFormatter formatter(stream);

        std::vector<std::pair<NascentObjectGuid, RenderCore::Assets::RawMaterial>> compiledEffects;

        const auto& effects = model._doc->_effects;
        for (auto i=effects.cbegin(); i!=effects.cend(); ++i) {
            TRY
            {
                NascentObjectGuid id = i->GetId().GetHash();
                compiledEffects.insert(
                    LowerBound(compiledEffects, id), 
                    std::make_pair(id, Convert(*i, model._resolveContext, model._cfg)));
            } CATCH (...) {
            } CATCH_END
        }

        const auto& mats = model._doc->_materials;
        for (auto m=mats.cbegin(); m!=mats.cend(); ++m) {
            GuidReference effect(m->_effectReference);
            auto i = LowerBound(compiledEffects, NascentObjectGuid(effect._id, effect._fileHash));
            if (i == compiledEffects.end() || !(i->first == NascentObjectGuid(effect._id, effect._fileHash)))
                continue;

            auto ele = formatter.BeginElement(m->_name.AsString());
            i->second.Serialize(formatter);
            formatter.EndElement(ele);
        }
    }

    std::vector<::Assets::ICompileOperation::OperationResult> SerializeMaterials(const ColladaCompileOp& model, StringSection<utf8> rootNodeName)
    { 
        // std::string matSettingsFile;
        // {
        //     ::Assets::ResChar settingsName[MaxPath];
        //     XlBasename(settingsName, dimof(settingsName), model._name.c_str());
        //     XlChopExtension(settingsName);
        //     XlCatString(settingsName, dimof(settingsName), ".material");
        //     matSettingsFile = settingsName;
        // }

        MemoryOutputStream<uint8> strm;
        SerializeMatTable(strm, model);
        return {
			::Assets::ICompileOperation::OperationResult{
				ChunkType_Text, 0, model._name,
				::Assets::AsBlob(MakeIteratorRange(strm.GetBuffer().Begin(), strm.GetBuffer().End()))}
		};
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PreparedAnimationFile
    {
    public:
        NascentAnimationSet _animationSet;
        std::vector<RenderCore::Assets::RawAnimationCurve> _curves;

        PreparedAnimationFile(const ColladaCompileOp&);
    };

    PreparedAnimationFile::PreparedAnimationFile(const ColladaCompileOp& input)
    {
        std::vector<UnboundAnimation> anims;
        const auto& animations = input._doc->_animations;
        for (auto i=animations.cbegin(); i!=animations.cend(); ++i) {
            TRY {
                auto anim = Convert(*i, input._resolveContext); 

                for (auto c=anim._curves.begin(); c!=anim._curves.end(); ++c) {
                    _curves.emplace_back(std::move(c->_curve));
                    _animationSet.AddAnimationDriver(
                        c->_parameterName, unsigned(_curves.size()-1),
                        c->_samplerType, c->_samplerOffset);
                }
            } CATCH (...) {
            } CATCH_END
        }

		// Add a default animation containing all of the drivers in this file
		if (!_animationSet.GetAnimationDrivers().empty() || !_animationSet.GetConstantDrivers().empty()) {
			_animationSet.AddAnimation(
				"main",
				0, (unsigned)_animationSet.GetAnimationDrivers().size(),
				0, (unsigned)_animationSet.GetConstantDrivers().size(),
				0.f, 5.0f);
		}
    }

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeAnimations(const ColladaCompileOp& model, StringSection<utf8> rootNodeName)
	{
		PreparedAnimationFile animFile(model);
		return SerializeAnimationsToChunks(
			model._name.c_str(), animFile._animationSet,
			MakeIteratorRange(animFile._curves));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static const uint64 Type_Model = ConstHash64<'Mode', 'l'>::Value;
	static const uint64 Type_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
	static const uint64 Type_Skeleton = ConstHash64<'Skel', 'eton'>::Value;
	static const uint64 Type_RawMat = ConstHash64<'RawM', 'at'>::Value;

	auto ColladaCompileOp::GetTargets() const -> std::vector<TargetDesc> 
	{
		return _targets;
	}

	auto	ColladaCompileOp::SerializeTarget(unsigned idx) -> std::vector<OperationResult>
	{
		if (idx >= _targets.size()) return {};

		switch (_targets[idx]._type) {
		case Type_Model:			return SerializeSkin(*this, MakeStringSection(_rootNode).Cast<utf8>());
		case Type_Skeleton:			return SerializeSkeleton(*this, MakeStringSection(_rootNode).Cast<utf8>());
		case Type_RawMat:			return SerializeMaterials(*this, MakeStringSection(_rootNode).Cast<utf8>());
		case Type_AnimationSet:		return SerializeAnimations(*this, MakeStringSection(_rootNode).Cast<utf8>());
		default:
			Throw(::Exceptions::BasicLabel("Cannot serialize target (%s)", _targets[idx]._name));
		}
	}

	std::vector<::Assets::DependentFileState> ColladaCompileOp::GetDependencies() const
	{
		return _dependencies;
	}

	ColladaCompileOp::ColladaCompileOp() {}
	ColladaCompileOp::~ColladaCompileOp() {}

	std::shared_ptr<::Assets::ICompileOperation> CreateNormalCompileOperation(StringSection<::Assets::ResChar> identifier)
	{
		std::shared_ptr<ColladaCompileOp> result = std::make_shared<ColladaCompileOp>();

		auto split = MakeFileNameSplitter(identifier);
		auto filePath = split.AllExceptParameters().AsString();

		result->_dependencies.push_back({ MakeStringSection("colladaimport.cfg"), ::Assets::MainFileSystem::TryGetDesc("colladaimport.cfg")._modificationTime });
		result->_dependencies.push_back({ filePath, ::Assets::MainFileSystem::TryGetDesc(filePath)._modificationTime });

		result->_cfg = ImportConfiguration("colladaimport.cfg");
		result->_fileData = ::Assets::MainFileSystem::OpenMemoryMappedFile(MakeStringSection(filePath), 0, "r", FileShareMode::Read);
		XmlInputStreamFormatter<utf8> formatter(
			MemoryMappedInputStream(result->_fileData.GetData()));
		formatter._allowCharacterData = true;

		result->_name = identifier.AsString();
		result->_rootNode = split.Parameters().AsString();
		result->_doc = std::make_shared<ColladaConversion::DocumentScaffold>();
		result->_doc->Parse(formatter);

		result->_resolveContext = ::ColladaConversion::URIResolveContext(result->_doc);

		result->_targets.push_back(ColladaCompileOp::TargetDesc{Type_Model, "Model"});
		result->_targets.push_back(ColladaCompileOp::TargetDesc{Type_RawMat, "RawMat"});
		result->_targets.push_back(ColladaCompileOp::TargetDesc{Type_Skeleton, "Skeleton"});
		result->_targets.push_back(ColladaCompileOp::TargetDesc{Type_AnimationSet, "Animations"});

		return std::move(result);
	}

	class MergedAnimCompileOp : public ::Assets::ICompileOperation
    {
    public:
		std::vector<TargetDesc> _targets;
		std::vector<::Assets::DependentFileState> _dependencies;

		NascentAnimationSet _animationSet;
        std::vector<RenderCore::Assets::RawAnimationCurve> _curves;

		std::vector<TargetDesc> GetTargets() const { return _targets; }
		std::vector<::Assets::DependentFileState> GetDependencies() const { return _dependencies; }

		std::vector<OperationResult> SerializeTarget(unsigned idx)
		{
			return SerializeAnimationsToChunks(
				"MergedAnimSet", _animationSet,
				MakeIteratorRange(_curves));
		}
    };

	std::shared_ptr<::Assets::ICompileOperation> CreateMergedAnimSetCompileOperation(StringSection<::Assets::ResChar> identifier, bool folderSearch)
	{
		// Search the given directory for all .dae files. We'll merge them all together as a single animation set
		std::vector<std::pair<std::string, std::string>> sourceFiles;
		if (folderSearch) {
			auto rawFiles = RawFS::FindFiles(identifier.AsString() + "/*.dae", RawFS::FindFilesFilter::File);
			for (const auto&filePath:rawFiles)
				sourceFiles.push_back(std::make_pair(filePath, MakeFileNameSplitter(filePath).File().AsString()));
		} else {
			auto cfgFileData = ::Assets::MainFileSystem::OpenMemoryMappedFile(identifier, 0, "r", FileShareMode::Read);
			InputStreamFormatter<utf8> formatter { MemoryMappedInputStream{cfgFileData.GetData()} };
			auto searchRules = ::Assets::DefaultDirectorySearchRules(identifier);
			for (;;) {
				StringSection<utf8> name, value;
				auto next = formatter.PeekNext();
				if (next == InputStreamFormatter<utf8>::Blob::AttributeName) {
					if (!formatter.TryAttribute(name, value))
						Throw(FormatException("Malformed attribute", formatter.GetLocation()));
					char foundFile[MaxPath];
					searchRules.ResolveFile(foundFile, value.Cast<char>());
					sourceFiles.push_back(std::make_pair(foundFile, name.Cast<char>().AsString()));
					continue;
				} else if (next == InputStreamFormatter<utf8>::Blob::EndElement || next == InputStreamFormatter<utf8>::Blob::None)
					break;

				Throw(FormatException("Unexpected blob", formatter.GetLocation()));
			}
		}

		std::shared_ptr<MergedAnimCompileOp> result = std::make_shared<MergedAnimCompileOp>();
		::Assets::DependentFileState cfgDep { MakeStringSection("colladaimport.cfg"), ::Assets::MainFileSystem::TryGetDesc("colladaimport.cfg")._modificationTime };
		result->_dependencies.push_back(cfgDep);

		ImportConfiguration cfg("colladaimport.cfg");

		for (const auto&filePath:sourceFiles) {
			::Assets::DependentFileState subFileDep { filePath.first, ::Assets::MainFileSystem::TryGetDesc(filePath.first)._modificationTime };
			result->_dependencies.push_back(subFileDep);

			ColladaCompileOp subResult;
			subResult._cfg = cfg;
			subResult._fileData = ::Assets::MainFileSystem::OpenMemoryMappedFile(MakeStringSection(filePath.first), 0, "r", FileShareMode::Read);
			XmlInputStreamFormatter<utf8> formatter(
				MemoryMappedInputStream(subResult._fileData.GetData()));
			formatter._allowCharacterData = true;

			subResult._name = identifier.AsString();
			subResult._doc = std::make_shared<ColladaConversion::DocumentScaffold>();
			subResult._doc->Parse(formatter);

			subResult._resolveContext = ::ColladaConversion::URIResolveContext(subResult._doc);

			PreparedAnimationFile animFile(subResult);

			result->_animationSet.MergeAnimation(
				animFile._animationSet, filePath.second, 
				animFile._curves, result->_curves);
		}

		result->_targets.push_back(ColladaCompileOp::TargetDesc{Type_AnimationSet, "Animations"});
		return result;
	}

	std::shared_ptr<::Assets::ICompileOperation> CreateCompileOperation(StringSection<::Assets::ResChar> identifier)
	{
#pragma comment(linker, "/EXPORT:CreateCompileOperation=" __FUNCDNAME__)
		auto splitter = MakeFileNameSplitter(identifier);
		if (XlEqStringI(splitter.FileAndExtension(), "alldae")) {
			return CreateMergedAnimSetCompileOperation(splitter.DriveAndPath(), true);
		} else if (XlEqStringI(splitter.Extension(), "daelst")) {
			return CreateMergedAnimSetCompileOperation(identifier, false);
		} else {
			return CreateNormalCompileOperation(identifier);
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static uint64_t s_knownAssetTypes[] = { Type_Model, Type_RawMat, Type_Skeleton, Type_AnimationSet };
	static uint64_t s_animSetAssetTypes[] = { Type_AnimationSet };

	class CompilerDesc : public ::Assets::ICompilerDesc
	{
	public:
		const char*			Description() const { return "Compiler and converter for Collada asset files"; }

		virtual unsigned	FileKindCount() const { return 3; }
		virtual FileKind	GetFileKind(unsigned index) const
		{
			assert(index == 0 || index == 1 || index == 2);
			if (index == 0)
				return FileKind { MakeIteratorRange(s_knownAssetTypes), R"(.*\.dae)", "Collada XML asset", "dae" };

			if (index == 1)
				return FileKind { MakeIteratorRange(s_animSetAssetTypes), R"(.*\.daelst)", "Animation List", "daelst" };

			return FileKind { MakeIteratorRange(s_animSetAssetTypes), R"(.*[\\/]alldae)", "All collada animations in a directory", "folder" };
		}

		CompilerDesc() {}
		~CompilerDesc() {}
	};

	std::shared_ptr<::Assets::ICompilerDesc> GetCompilerDesc() 
	{
#pragma comment(linker, "/EXPORT:GetCompilerDesc=" __FUNCDNAME__)
		return std::make_shared<CompilerDesc>();
	}

}

extern "C" 
{

	dll_export ConsoleRig::LibVersionDesc GetVersionInformation()
	{
		return ConsoleRig::GetLibVersionDesc();
	}

	static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> s_attachRef;

	dll_export void AttachLibrary(ConsoleRig::CrossModule& crossModule)
	{
		ConsoleRig::CrossModule::SetInstance(crossModule);
		s_attachRef = ConsoleRig::GetAttachablePtr<ConsoleRig::GlobalServices>();
		auto versionDesc = ConsoleRig::GetLibVersionDesc();
		Log(Verbose) << "Attached Collada Compiler DLL: {" << versionDesc._versionString << "} -- {" << versionDesc._buildDateString << "}" << std::endl;
	}

	dll_export void DetachLibrary()
	{
		s_attachRef.reset();
		ConsoleRig::CrossModule::ReleaseInstance();
	}

}
