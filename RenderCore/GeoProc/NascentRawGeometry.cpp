// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentRawGeometry.h"
#include "../Format.h"
#include "../Assets/AssetUtils.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StreamUtils.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
    void NascentRawGeometry::Serialize(
        Serialization::NascentBlockSerializer& outputSerializer, 
        std::vector<uint8>& largeResourcesBlock) const
    {
            //  We're going to write the index and vertex buffer data to the "large resources block"
            //  class members and scaffold structure get written to the serialiser, but the very large stuff
            //  should end up in a separate pool

        auto vbOffset = largeResourcesBlock.size();
        auto vbSize = _vertices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _vertices.begin(), _vertices.end());

        auto ibOffset = largeResourcesBlock.size();
        auto ibSize = _indices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _indices.begin(), _indices.end());

        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawInputAssembly, unsigned(vbOffset), unsigned(vbSize) });

        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::IndexData 
                { _indexFormat, unsigned(ibOffset), unsigned(ibSize) });
        
        ::Serialize(outputSerializer, _mainDrawCalls);
    }

    std::ostream& StreamOperator(std::ostream& stream, const NascentRawGeometry& geo)
    {
        using namespace RenderCore::Assets::Operators;
        stream << "Vertex bytes: " << ByteCount(geo._vertices.size()) << std::endl;
        stream << "Index bytes: " << ByteCount(geo._indices.size()) << std::endl;
        stream << "IA: " << geo._mainDrawInputAssembly << std::endl;
        stream << "Index fmt: " << AsString(geo._indexFormat) << std::endl;
        unsigned c=0;
        for(const auto& dc:geo._mainDrawCalls) {
            stream << "Draw [" << c++ << "] " << dc << std::endl;
        }
        
        stream << "Material binding: ";
        for (size_t q=0; q<geo._matBindingSymbols.size(); ++q) {
            if (q != 0) stream << ", ";
            stream << geo._matBindingSymbols[q];
        }
        stream << std::endl;

        return stream;
    }

}}}


