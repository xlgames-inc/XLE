// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ColladaConversion.h"
#include "RawGeometry.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"

namespace COLLADAFW { class UniqueId; class Image; class SkinControllerData; }

namespace RenderCore { namespace ColladaConversion
{

        ////////////////////////////////////////////////////////

    class NascentBoundSkinnedGeometry
    {
    public:
        DynamicArray<uint8>         _unanimatedVertexElements;
        DynamicArray<uint8>         _indices;

        Metal::NativeFormat::Enum   _indexFormat;        
        GeometryInputAssembly       _mainDrawUnanimatedIA;
        GeometryInputAssembly       _mainDrawAnimatedIA;

        std::vector<NascentDrawCallDesc>    _mainDrawCalls;

        DynamicArray<uint8>         _animatedVertexElements;
        DynamicArray<uint8>         _skeletonBinding;
        unsigned                    _skeletonBindingVertexStride;
        unsigned                    _animatedVertexBufferSize;

        DynamicArray<Float4x4>      _inverseBindMatrices;
        DynamicArray<uint16>        _jointMatrices;         // (uint16 or uint8 for this array)
        Float4x4                    _bindShapeMatrix;
            
        std::vector<NascentDrawCallDesc>    _preskinningDrawCalls;
        GeometryInputAssembly               _preskinningIA;

        std::pair<Float3, Float3>           _localBoundingBox;

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;

        NascentBoundSkinnedGeometry(DynamicArray<uint8>&&   unanimatedVertexElements,
                                    DynamicArray<uint8>&&   animatedVertexElements,
                                    DynamicArray<uint8>&&   skeletonBinding,
                                    DynamicArray<uint8>&&   indices);
        NascentBoundSkinnedGeometry(NascentBoundSkinnedGeometry&& moveFrom);
        NascentBoundSkinnedGeometry& operator=(NascentBoundSkinnedGeometry&& moveFrom);
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
            std::vector<Metal::InputElementDesc>    _vertexInputLayout;
            unsigned                                _weightCount;

            std::unique_ptr<uint8[]>                _vertexBufferData;
            size_t                                  _vertexBufferSize;

            std::vector<uint16>                     _vertexBindings;

            Bucket();
            Bucket(Bucket&& moveFrom);
            Bucket& operator=(Bucket&& moveFrom) never_throws;

        private:
            Bucket& operator=(const Bucket& copyFrom);
        };

        DynamicArray<Float4x4>  _inverseBindMatrices;
        Float4x4                _bindShapeMatrix;

        Bucket                  _bucket[4];      // 4, 2, 1, 0
        std::vector<uint32>     _positionIndexToBucketIndex;

        UnboundSkinController(  Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0, 
                                DynamicArray<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
                                std::vector<uint32>&& vertexPositionToBucketIndex);
        UnboundSkinController(UnboundSkinController&& moveFrom);
        UnboundSkinController& operator=(UnboundSkinController&& moveFrom) never_throws;

    private:
        UnboundSkinController& operator=(const UnboundSkinController& copyFrom);
    };

        ////////////////////////////////////////////////////////

    class FullColladaId
    {
    public:
        unsigned        _classId;
        uint64          _objectId;
        unsigned        _fileId;

        COLLADAFW::UniqueId     AsColladaId() const;
        FullColladaId(const COLLADAFW::UniqueId& input);
        FullColladaId();
    };

        ////////////////////////////////////////////////////////

    class UnboundMorphController
    {
    public:
        FullColladaId   _source;

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
        ObjectId        _unboundControllerId;
        FullColladaId   _source;
        std::vector<HashedColladaUniqueId>  _jointIds;
    };

        ////////////////////////////////////////////////////////

    class JointReferences
    {
    public: 
        struct Reference
        {
            HashedColladaUniqueId   _joint;
            Float4x4                _inverseBindMatrix;
        };
        std::vector<Reference>      _references;

        bool HasJoint(HashedColladaUniqueId joint) const;
    };

        ////////////////////////////////////////////////////////

    class ReferencedTexture
    {
    public:
        std::string     _resourceName;
        ReferencedTexture(const std::string& resourceName) : _resourceName(resourceName) {}
    };

    class ReferencedMaterial
    {
    public:
        FullColladaId   _effectId;
        ReferencedMaterial(const FullColladaId& effectId) : _effectId(effectId) {}
    };

        ////////////////////////////////////////////////////////

    ReferencedTexture       Convert(const COLLADAFW::Image* image);
    UnboundSkinController   Convert(const COLLADAFW::SkinControllerData* input);

}}

