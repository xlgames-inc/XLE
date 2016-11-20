// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ModelScaffoldInternal.h"
#include "../../RenderCore/Types_Forward.h"
#include "../../Utility/PtrUtils.h"            // for DynamicArray
#include <vector>

namespace Serialization { class NascentBlockSerializer; }

namespace RenderCore { namespace Assets { namespace GeoProc
{
    using GeoInputAssembly = RenderCore::Assets::GeoInputAssembly;
    using DrawCallDesc = RenderCore::Assets::DrawCallDesc;

        ////////////////////////////////////////////////////////

    class NascentRawGeometry
    {
    public:
        void Serialize(
            Serialization::NascentBlockSerializer& outputSerializer, 
            std::vector<uint8>& largeResourcesBlock) const;

        NascentRawGeometry(
            DynamicArray<uint8>&& vb, DynamicArray<uint8>&& ib,
            GeoInputAssembly&&              mainDrawInputAssembly,
            Format                          indexFormat,
            std::vector<DrawCallDesc>&&     mainDrawCalls,
            DynamicArray<uint32>&&          unifiedVertexIndexToPositionIndex,
            std::vector<uint64>&&           matBindingSymbols);
        NascentRawGeometry(NascentRawGeometry&& moveFrom);
        NascentRawGeometry& operator=(NascentRawGeometry&& moveFrom);
        NascentRawGeometry();

        DynamicArray<uint8>         _vertices;
        DynamicArray<uint8>         _indices;

        GeoInputAssembly            _mainDrawInputAssembly;
        Format                      _indexFormat;
        std::vector<DrawCallDesc>   _mainDrawCalls;
        std::vector<uint64>         _matBindingSymbols;

            //  Only required during processing
        DynamicArray<uint32>        _unifiedVertexIndexToPositionIndex;

        NascentRawGeometry(NascentRawGeometry&) = delete;
        NascentRawGeometry& operator=(const NascentRawGeometry&) = delete;

        friend std::ostream& StreamOperator(std::ostream&, const NascentRawGeometry&);
    };

}}}

