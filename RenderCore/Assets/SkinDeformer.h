// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelScaffoldInternal.h"
#include "SkeletonScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "../Format.h"
#include "../Assets/SimpleModelDeform.h"
#include "../Math/Vector.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

namespace RenderCore { class VertexElementIterator; }

namespace RenderCore { namespace Assets
{
	class SkinDeformer : public RenderCore::Assets::IDeformOperation
	{
	public:
		virtual void Execute(IteratorRange<const VertexElementRange*> destinationElements) const;

		void FeedInSkeletonMachineResults(
			IteratorRange<const Float4x4*> skeletonMachineOutput,
			const SkeletonMachine::OutputInterface& skeletonMachineOutputInterface);
		
		SkinDeformer(
			const RenderCore::Assets::ModelScaffold& modelScaffold,
			unsigned geoId);
		~SkinDeformer();

		static void Register();
	private:
		std::vector<Float3> _basePositions;
		std::vector<Float4> _jointWeights;
		std::vector<UInt4>	_jointIndices;

		ModelCommandStream::InputInterface _jointInputInterface;

		IteratorRange<const DrawCallDesc*> _preskinningDrawCalls;
		IteratorRange<const Float4x4*> _bindShapeByInverseBindMatrices;
		Float4x4 _bindShapeMatrix;
		IteratorRange<const uint16_t*> _jointMatrices;
		std::vector<Float4x4> _skeletonMachineOutput;
		SkeletonBinding _skeletonBinding;

		void WriteJointTransforms(	IteratorRange<Float3x4*>		destination,
									IteratorRange<const Float4x4*>	skeletonMachineResult) const;
	};
}}

