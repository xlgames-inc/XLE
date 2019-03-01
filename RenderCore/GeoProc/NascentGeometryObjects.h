// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentRawGeometry.h"
#include "NascentAnimController.h"
#include <vector>
#include <iosfwd>

namespace RenderCore { namespace Assets { namespace GeoProc
{

	class NascentGeometryObjects
	{
	public:
		std::vector<std::pair<NascentObjectGuid, NascentRawGeometry>> _rawGeos;
		std::vector<std::pair<NascentObjectGuid, NascentBoundSkinnedGeometry>> _skinnedGeos;

		std::pair<Float3, Float3> CalculateBoundingBox
		(
			const NascentModelCommandStream& scene,
			IteratorRange<const Float4x4*> transforms
		) const;

		friend std::ostream& operator<<(std::ostream&, const NascentGeometryObjects& geos);
	};

}}}

