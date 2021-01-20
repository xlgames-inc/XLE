// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetUtils.h"
#include "ModelScaffoldInternal.h"
#include "../Types.h"

namespace RenderCore { namespace Assets
{
    std::ostream& SerializationOperator(std::ostream& stream, const GeoInputAssembly& ia)
    {
        // todo -- maybe check for common vertex formats (eg PNT, etc) and give them names here?
        stream << "{ GeoInputAssembly }";
        return stream;
    }

    std::ostream& SerializationOperator(std::ostream& stream, const DrawCallDesc& dc)
    {
        stream << "{ [" << AsString(dc._topology) << "] idxCount: " << dc._indexCount;
        if (dc._firstIndex)
            stream << ", firstIdx: " << dc._firstIndex;
        stream << ", material: " << dc._subMaterialIndex;
        stream << ", topology: " << dc._subMaterialIndex;
        stream << " }";
        return stream;
    }

}}

