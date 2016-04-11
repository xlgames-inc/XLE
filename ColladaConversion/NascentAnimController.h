// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentRawGeometry.h"
#include "SkeletonRegistry.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/PtrUtils.h"
#include <vector>

namespace RenderCore { namespace ColladaConversion
{

        ////////////////////////////////////////////////////////

    class NascentBoundSkinnedGeometry
    {
    public:
        DynamicArray<uint8>         _unanimatedVertexElements;
        DynamicArray<uint8>         _indices;

        Format                      _indexFormat;        
        GeoInputAssembly            _mainDrawUnanimatedIA;
        GeoInputAssembly            _mainDrawAnimatedIA;

        std::vector<DrawCallDesc>    _mainDrawCalls;

        DynamicArray<uint8>         _animatedVertexElements;
        DynamicArray<uint8>         _skeletonBinding;
        unsigned                    _skeletonBindingVertexStride;
        unsigned                    _animatedVertexBufferSize;

        DynamicArray<Float4x4>      _inverseBindMatrices;
        DynamicArray<uint16>        _jointMatrices;         // (uint16 or uint8 for this array)
        Float4x4                    _bindShapeMatrix;
            
        std::vector<DrawCallDesc>   _preskinningDrawCalls;
        GeoInputAssembly            _preskinningIA;

        std::pair<Float3, Float3>           _localBoundingBox;

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;

        NascentBoundSkinnedGeometry(DynamicArray<uint8>&&   unanimatedVertexElements,
                                    DynamicArray<uint8>&&   animatedVertexElements,
                                    DynamicArray<uint8>&&   skeletonBinding,
                                    DynamicArray<uint8>&&   indices);
        NascentBoundSkinnedGeometry(NascentBoundSkinnedGeometry&& moveFrom);
        NascentBoundSkinnedGeometry& operator=(NascentBoundSkinnedGeometry&& moveFrom);

        friend std::ostream& StreamOperator(std::ostream&, const NascentBoundSkinnedGeometry&);
    };

        ////////////////////////////////////////////////////////

    class NascentModelCommandStream;
    class UnboundSkinController;
    class UnboundSkinControllerAndAttachedSkeleton;
    class NascentBoundSkinnedGeometry;
    class ReferencedTexture;
    class ReferencedMaterial;

        ////////////////////////////////////////////////////////

    class UnboundSkinController 
    {
    public:
        class Bucket
        {
        public:
            std::vector<InputElementDesc>    _vertexInputLayout;
            unsigned                         _weightCount;

            std::unique_ptr<uint8[]>         _vertexBufferData;
            size_t                           _vertexBufferSize;

            std::vector<uint16>              _vertexBindings;

            Bucket();
            Bucket(Bucket&& moveFrom) never_throws;
            Bucket& operator=(Bucket&& moveFrom) never_throws;

        private:
            Bucket& operator=(const Bucket& copyFrom);
        };

        Float4x4                _bindShapeMatrix;

        Bucket                  _bucket[4];      // 4, 2, 1, 0
        std::vector<uint32>     _positionIndexToBucketIndex;

        std::vector<std::basic_string<utf8>> _jointNames;
        DynamicArray<Float4x4>  _inverseBindMatrices;

        ObjectGuid _sourceRef;

        UnboundSkinController(  
            Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0, 
            DynamicArray<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
            std::vector<std::basic_string<utf8>>&& jointNames,
            ObjectGuid sourceRef,
            std::vector<uint32>&& vertexPositionToBucketIndex);
        UnboundSkinController(UnboundSkinController&& moveFrom) never_throws;
        UnboundSkinController& operator=(UnboundSkinController&& moveFrom) never_throws;

    private:
        UnboundSkinController& operator=(const UnboundSkinController& copyFrom);
    };

        ////////////////////////////////////////////////////////

    class UnboundMorphController
    {
    public:
        ObjectGuid   _source;

        UnboundMorphController();
        UnboundMorphController(UnboundMorphController&& moveFrom);
        UnboundMorphController& operator=(UnboundMorphController&& moveFrom) never_throws;
    private:
        UnboundMorphController& operator=(const UnboundMorphController& copyFrom);
    };

        ////////////////////////////////////////////////////////

    class UnboundSkinControllerAndAttachedSkeleton
    {
    public:
        ObjectGuid                  _unboundControllerId;
        ObjectGuid                  _source;
        std::vector<ObjectGuid>     _jointIds;
    };


        ////////////////////////////////////////////////////////

    class TableOfObjects;

    NascentBoundSkinnedGeometry BindController(
        const NascentRawGeometry& sourceGeo,
        const UnboundSkinController& controller,
        DynamicArray<uint16>&& jointMatrices,
        const char nodeName[]);
}}

