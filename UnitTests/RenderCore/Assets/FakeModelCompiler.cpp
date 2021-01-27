// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FakeModelCompiler.h"
#include "../../../RenderCore/Assets/ModelScaffold.h"
#include "../../../RenderCore/Assets/RawMaterial.h"
#include "../../../Utility/Streams/StreamTypes.h"
#include "../../../Utility/Streams/StreamFormatter.h"

namespace UnitTests
{
	static const uint64 Type_Model = ConstHash64<'Mode', 'l'>::Value;
	static const uint64 Type_RawMat = ConstHash64<'RawM', 'at'>::Value;
	static const uint64 Type_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
	static const uint64 Type_Skeleton = ConstHash64<'Skel', 'eton'>::Value;

	class FakeModelCompileOperation : public ::Assets::ICompileOperation
	{
	public:
		std::vector<::Assets::DependentFileState> GetDependencies() const override { return _dependencies; }
		std::vector<TargetDesc> GetTargets() const override { return _targets; }
		std::vector<SerializedArtifact> SerializeTarget(unsigned idx) override
		{
			if (idx >= _targets.size())
				return {};

			if (_targets[idx]._type == Type_Model)
				return SerializeModel();
			else if (_targets[idx]._type == Type_RawMat)
				return SerializeRawMat();
			else if (_targets[idx]._type == Type_Skeleton)
				return SerializeSkeleton();

			return {};
		}
		
		FakeModelCompileOperation()
		{
			// Normally we'd open the input file at this point and figure out
			// what it contains and what we can compile it into. There's no input
			// for this compile operation, though, so everything's just static

			// We are generally able to extract multiple different types of
			// asset information from a single compile of an input model file
			// That is we can get model data, as well as skeleton and material data
			// Some formats (eg, collada, fbx) will store different types of asset data
			// within the same container format. Ie, some files of this format might 
			// have model data, other might have animation.
			// This is why the ICompileOperation returns a list of the different 
			// "target" data types for this specific operation.
			_targets = {
				TargetDesc { Type_Model, "Model" },
				TargetDesc { Type_RawMat, "RawMat" },
				TargetDesc { Type_Skeleton, "Skeleton" }
			};

			_modelName = "fake-model";
		}
		~FakeModelCompileOperation()
		{}
	private:
		std::vector<::Assets::DependentFileState> _dependencies;
		std::vector<TargetDesc> _targets;
		std::string _modelName;

		std::vector<SerializedArtifact> SerializeModel();
		std::vector<SerializedArtifact> SerializeRawMat();
		std::vector<SerializedArtifact> SerializeSkeleton();
	};

	auto FakeModelCompileOperation::SerializeModel() -> std::vector<SerializedArtifact>
	{
		return {};
	}

	auto FakeModelCompileOperation::SerializeRawMat() -> std::vector<SerializedArtifact>
	{
		MemoryOutputStream<char> strm;

        {
			OutputStreamFormatter formatter(strm);

			RenderCore::Assets::RawMaterial material0;
			material0._constants = {
				std::make_pair("Emissive", "{.5, .5, .5}c"),
				std::make_pair("Brightness", "50")
			};
			RenderCore::Assets::RawMaterial material1;
			material1._constants = {
				std::make_pair("Emissive", "{2.5, .25, .15}c"),
				std::make_pair("Brightness", "33")
			};

			auto matContainer = formatter.BeginElement("Material0");
			formatter << material0;
			formatter.EndElement(matContainer);

			matContainer = formatter.BeginElement("Material1");
			formatter << material1;
			formatter.EndElement(matContainer);
		}

        return {
			::Assets::ICompileOperation::SerializedArtifact{
				Type_RawMat, 0, _modelName,
				::Assets::AsBlob(MakeIteratorRange(strm.GetBuffer().Begin(), strm.GetBuffer().End()))}
		};
	}

	auto FakeModelCompileOperation::SerializeSkeleton() -> std::vector<SerializedArtifact>
	{
		return {};
	}

	static std::shared_ptr<::Assets::ICompileOperation> BeginFakeModelCompilation(
		IteratorRange<const StringSection<>*> identifiers)
	{
		return std::make_shared<FakeModelCompileOperation>();
	}

	::Assets::IntermediateCompilers::CompilerRegistration RegisterFakeModelCompiler(
		::Assets::IntermediateCompilers& intermediateCompilers)
	{
		auto result = intermediateCompilers.RegisterCompiler(
			"fake-model-scaffold-compiler",
			ConsoleRig::GetLibVersionDesc(),
			nullptr,
			BeginFakeModelCompilation);

		uint64_t outputAssetTypes[] = { 
			Type_Model,
			Type_RawMat,
			Type_AnimationSet,
			Type_Skeleton,
		};
		intermediateCompilers.AssociateRequest(
			result._registrationId,
			MakeIteratorRange(outputAssetTypes),
			"fake-model");
		return result;
	}   
}