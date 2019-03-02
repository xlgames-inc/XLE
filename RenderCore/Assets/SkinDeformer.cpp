// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SkinDeformer.h"
#include "ModelScaffold.h"
#include "ModelScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/AssetTraits.h"
#include <assert.h>

namespace RenderCore { namespace Assets
{
	void SkinDeformer::WriteJointTransforms(	IteratorRange<Float3x4*>		destination,
												IteratorRange<const Float4x4*>	skeletonMachineResult) const
    {
        for (unsigned c=0; c<std::min(_jointMatrices.size(), destination.size()); ++c) {
            auto transMachineOutput = _skeletonBinding.ModelJointToMachineOutput(_jointMatrices[c]);
            if (transMachineOutput != ~unsigned(0x0)) {
                Float4x4 finalMatrix = Combine(_bindShapeByInverseBindMatrices[c], skeletonMachineResult[transMachineOutput]);
                destination[c] = Truncate(finalMatrix);
            } else {
                destination[c] = Identity<Float3x4>();
            }
        }
    }

	void SkinDeformer::FeedInSkeletonMachineResults(
		IteratorRange<const Float4x4*> skeletonMachineOutput,
		const SkeletonMachine::OutputInterface& skeletonMachineOutputInterface)
	{
		_skeletonMachineOutput = std::vector<Float4x4>{skeletonMachineOutput.begin(), skeletonMachineOutput.end()};
		_skeletonBinding = SkeletonBinding{skeletonMachineOutputInterface, _jointInputInterface};
	}

	void SkinDeformer::Execute(IteratorRange<const VertexElementRange*> destinationElements) const
	{
		assert(destinationElements.size() == 1);

		auto& posElement = destinationElements[0];
		assert(posElement.begin().Format() == Format::R32G32B32_FLOAT);
		assert(posElement.size() <= _basePositions.size());

		std::vector<Float3x4> jointTransform(_jointMatrices.size());
		WriteJointTransforms(
			MakeIteratorRange(jointTransform),
			MakeIteratorRange(_skeletonMachineOutput));

		for (const auto&drawCall:_preskinningDrawCalls) {
			assert((drawCall._firstVertex + drawCall._indexCount) <= posElement.size());

			auto srcPosition = _basePositions.begin() + drawCall._firstVertex;

			// drawCall._subMaterialIndex is 0, 1, 2 or 4 depending on the number of weights we have to proces
			if (drawCall._subMaterialIndex == 0) {
				// in this case, we just copy
				for (auto p=posElement.begin() + drawCall._firstVertex; p < (posElement.begin() + drawCall._firstVertex + drawCall._indexCount); ++p, ++srcPosition) 
					*p = *srcPosition;
				continue;
			}

			auto srcJointWeight = _jointWeights.begin() + drawCall._firstVertex;
			auto srcJointIndex = _jointIndices.begin() + drawCall._firstVertex;

			for (auto p=posElement.begin() + drawCall._firstVertex; 
				p < (posElement.begin() + drawCall._firstVertex + drawCall._indexCount); 
				++p, ++srcPosition, ++srcJointWeight, ++srcJointIndex) {
				
				Float3 deformedPosition { 0.f, 0.f, 0.f };
				for (unsigned b=0; b<drawCall._subMaterialIndex; ++b) {
					assert((*srcJointIndex)[b] < jointTransform.size());
					deformedPosition += (*srcJointWeight)[b] * TransformPoint(jointTransform[(*srcJointIndex)[b]], *srcPosition);
				}

				*p = deformedPosition;
			}
		}
	}

	static IteratorRange<VertexElementIterator> AsVertexElementIteratorRange(
		IteratorRange<void*> vbData,
		const RenderCore::Assets::VertexElement& ele,
		unsigned vertexStride)
	{
		VertexElementIterator begin {
			MakeIteratorRange(PtrAdd(vbData.begin(), ele._alignedByteOffset), AsPointer(vbData.end())),
			vertexStride, ele._nativeFormat };
		VertexElementIterator end {
			MakeIteratorRange(AsPointer(vbData.end()), AsPointer(vbData.end())),
			vertexStride, ele._nativeFormat };
		return { begin, end };
	}

	static const RenderCore::Assets::VertexElement* FindElement(
		IteratorRange<const RenderCore::Assets::VertexElement*> ele,
		StringSection<> semantic, unsigned semanticIndex = 0)
	{
		return std::find_if(
			ele.begin(), ele.end(),
			[semantic, semanticIndex](const RenderCore::Assets::VertexElement& ele) {
				return XlEqString(semantic, ele._semanticName) && ele._semanticIndex == semanticIndex;
			});
	}

	SkinDeformer::SkinDeformer(
		const RenderCore::Assets::ModelScaffold& modelScaffold,
		unsigned geoId)
	{
		auto& immData = modelScaffold.ImmutableData();
		assert(geoId < immData._boundSkinnedControllerCount);
		auto& skinnedController = immData._boundSkinnedControllers[geoId];
		auto& animVb = skinnedController._animatedVertexElements;
		auto& skelVb = skinnedController._skeletonBinding;

		auto positionElement = FindElement(MakeIteratorRange(animVb._ia._elements), "POSITION");
		auto weightsElement = FindElement(MakeIteratorRange(skelVb._ia._elements), "WEIGHTS");
		auto jointIndicesElement = FindElement(MakeIteratorRange(skelVb._ia._elements), "JOINTINDICES");
		if (positionElement == animVb._ia._elements.end() || weightsElement == skelVb._ia._elements.end() || jointIndicesElement == skelVb._ia._elements.end())
			Throw(std::runtime_error("Could not create SkinDeformer because there is no position, weights and/or joint indices element in input geometry"));

		auto animVbData = std::make_unique<uint8_t[]>(animVb._size);
		auto skelVbData = std::make_unique<uint8_t[]>(skelVb._size);
		{
			auto largeBlocks = modelScaffold.OpenLargeBlocks();
			auto base = largeBlocks->TellP();
			largeBlocks->Seek(base + animVb._offset);
			largeBlocks->Read(animVbData.get(), animVb._size);

			largeBlocks->Seek(base + skelVb._offset);
			largeBlocks->Read(skelVbData.get(), skelVb._size);
		}

		_basePositions = AsFloat3s(AsVertexElementIteratorRange(MakeIteratorRange(animVbData.get(), PtrAdd(animVbData.get(), animVb._size)), *positionElement, animVb._ia._vertexStride));
		_jointWeights = AsFloat4s(AsVertexElementIteratorRange(MakeIteratorRange(skelVbData.get(), PtrAdd(skelVbData.get(), skelVb._size)), *weightsElement, skelVb._ia._vertexStride));
		_jointIndices = AsUInt4s(AsVertexElementIteratorRange(MakeIteratorRange(skelVbData.get(), PtrAdd(skelVbData.get(), skelVb._size)), *jointIndicesElement, skelVb._ia._vertexStride));

		_preskinningDrawCalls = { skinnedController._preskinningDrawCalls, skinnedController._preskinningDrawCalls + skinnedController._preskinningDrawCallCount };
		_bindShapeByInverseBindMatrices = { skinnedController._bindShapeByInverseBindMatrices, skinnedController._bindShapeByInverseBindMatrices + skinnedController._bindShapeByInverseBindMatrixCount };
		_jointMatrices = { skinnedController._jointMatrices, skinnedController._jointMatrices + skinnedController._jointMatrixCount };

		_jointInputInterface = modelScaffold.CommandStream().GetInputInterface();
	}

	SkinDeformer::~SkinDeformer()
	{
	}

	std::vector<RenderCore::Assets::DeformOperationInstantiation> CreateSkinDeformAttachments(
		StringSection<> initializer,
		const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold)
	{
		// auto sep = std::find(initializer.begin(), initializer.end(), ',');
		// assert(sep != initializer.end());

		auto positionEle = Hash64("POSITION");
		auto weightsEle = Hash64("WEIGHTS");
		auto jointIndicesEle = Hash64("JOINTINDICES");
		std::vector<RenderCore::Assets::DeformOperationInstantiation> result;
		auto& immData = modelScaffold->ImmutableData();
		for (unsigned c=0; c<immData._boundSkinnedControllerCount; ++c) {

			// skeleton & anim set:
			//			StringSection<>(initializer.begin(), sep),
			//			StringSection<>(sep+1, initializer.end()

			result.push_back(
				RenderCore::Assets::DeformOperationInstantiation {
					std::make_shared<SkinDeformer>(*modelScaffold, c),
					unsigned(immData._geoCount) + c,
					{MiniInputElementDesc{positionEle, Format::R32G32B32_FLOAT}},
					{positionEle, weightsEle, jointIndicesEle}
				});
		}

		return result;
	}

	void SkinDeformer::Register()
	{
		auto& factory = RenderCore::Assets::DeformOperationFactory::GetInstance();
		factory.RegisterDeformOperation("skin", CreateSkinDeformAttachments);
	}

}}
