// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentRawGeometry.h"
#include "NascentAnimController.h"
#include <vector>

namespace RenderCore { namespace ColladaConversion {

	class NascentGeometryObjects
	{
	public:
		std::vector<std::pair<ObjectGuid, NascentRawGeometry>> _rawGeos;
		std::vector<std::pair<ObjectGuid, NascentBoundSkinnedGeometry>> _skinnedGeos;

		unsigned GetGeo(ObjectGuid id);
		unsigned GetSkinnedGeo(ObjectGuid id);

		std::pair<Float3, Float3> CalculateBoundingBox
		(
			const NascentModelCommandStream& scene,
			IteratorRange<const Float4x4*> transforms
		) const;

		friend std::ostream& operator<<(std::ostream&, const NascentGeometryObjects& geos);
	};

}}

