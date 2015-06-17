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
#include "../Utility/UTFUtils.h"

namespace COLLADAFW { class UniqueId; class Image; class SkinControllerData; }

namespace RenderCore { namespace Assets { using AnimationParameterId = uint32; }}

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

    class ObjectGuid
    {
    public:
        uint64  _objectId;
        uint64  _fileId;

        ObjectGuid() : _objectId(~0ull), _fileId(~0ull) {}
        ObjectGuid(uint64 objectId, uint64 fileId = 0) : _objectId(objectId), _fileId(fileId) {}
    };

    inline bool operator==(const ObjectGuid& lhs, const ObjectGuid& rhs)   { return (lhs._objectId == rhs._objectId) && (lhs._fileId == rhs._fileId); }
    inline bool operator<(const ObjectGuid& lhs, const ObjectGuid& rhs)    { if (lhs._fileId < rhs._fileId) return true; return lhs._objectId < rhs._objectId; }

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

    class NodeReferences
    {
    public:
        bool        IsImportant(ObjectGuid node) const;
        unsigned    GetOutputMatrixIndex(ObjectGuid node);

        const Float4x4* GetInverseBindMatrix(ObjectGuid node) const;
        void    AttachInverseBindMatrix(ObjectGuid node, const Float4x4& inverseBind);

        void    MarkParameterAnimated(const std::string& paramName);
        bool    IsAnimated(const std::string& paramName) const;

        NodeReferences();
        ~NodeReferences();
    protected:
        using OutputMatrixIndex = unsigned;
        static const OutputMatrixIndex OutputMatrixIndex_UnSet = ~unsigned(0);

        std::vector<std::pair<ObjectGuid, OutputMatrixIndex>> _nodeReferences;
        std::vector<std::pair<ObjectGuid, Float4x4>> _inverseBindMatrics;
        std::vector<std::string> _markParameterAnimated;

        OutputMatrixIndex _nextOutputIndex;
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
        typedef uint64 Guid;
        ObjectGuid      _effectId;
        Guid            _guid;
        std::string     _descriptiveName;

        ReferencedMaterial(
            const ObjectGuid& effectId,
            const Guid& guid,
            const std::string& descriptiveName)
        : _effectId(effectId), _guid(guid), _descriptiveName(descriptiveName) {}
    };

        ////////////////////////////////////////////////////////

    ObjectGuid              Convert(const COLLADAFW::UniqueId& input);
    ReferencedTexture       Convert(const COLLADAFW::Image* image);
    UnboundSkinController   Convert(const COLLADAFW::SkinControllerData* input);

    RenderCore::Assets::AnimationParameterId BuildAnimParameterId(const COLLADAFW::UniqueId& input);

    class TableOfObjects;

    NascentBoundSkinnedGeometry InstantiateSkinnedController(
        const NascentRawGeometry& sourceGeo,
        const UnboundSkinController& controller,
        const TableOfObjects& accessableObjects, 
        TableOfObjects& destinationForNewObjects,
        DynamicArray<uint16>&& jointMatrices,
        const char nodeName[]);
}}

