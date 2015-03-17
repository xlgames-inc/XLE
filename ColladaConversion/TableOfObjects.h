// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ColladaConversion.h"
#include "../../../Utility/Mixins.h"

namespace Serialization { class NascentBlockSerializer; }

namespace RenderCore { namespace Assets { class RawMaterial; class RawAnimationCurve; } }
namespace Utility { class Data; }

namespace RenderCore { namespace ColladaConversion
{
    class NascentRawGeometry;
    class NascentModelCommandStream;
    class UnboundSkinController;
    class UnboundSkinControllerAndAttachedSkeleton;
    class UnboundMorphController;
    class NascentBoundSkinnedGeometry;
    class ReferencedTexture;
    class ReferencedMaterial;

    class TableOfObjects : noncopyable
    {
    public:
        template <typename Type>
            ObjectId    GetObjectId(const COLLADAFW::UniqueId&      id) const;

        template <typename Type>
            bool        Has(const COLLADAFW::UniqueId&      id) const;

        template <typename Type>
            ObjectId    Add(    const std::string&          idString, 
                                const std::string&          name,
                                const COLLADAFW::UniqueId&  id,
                                Type&&                      object);

        template <typename Type>
            const Type* GetFromObjectId(ObjectId id) const never_throws;

        template <typename Type>
            std::tuple<std::string, std::string, COLLADAFW::UniqueId> 
                GetDesc(ObjectId id) const never_throws;

        void    SerializeSkin(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;
        void    SerializeAnimationSet(Serialization::NascentBlockSerializer& outputSerializer) const;
        std::vector<std::unique_ptr<Data>>  SerializeMaterial() const;

        TableOfObjects();
        ~TableOfObjects();
        TableOfObjects(TableOfObjects&& moveFrom);
        TableOfObjects& operator=(TableOfObjects&& moveFrom) never_throws;
    private:
        template<typename Type> class Object
        {
        public:
            std::string             _idString, _name;
            COLLADAFW::UniqueId     _hashId;
            Type                    _internalType;

            Object(const std::string& idString, const std::string& name, const COLLADAFW::UniqueId& hashId, Type&& internalType);
            ~Object();
            Object(Object&& cloneFrom) never_throws;
            Object<Type>& operator=(Object&& cloneFrom) never_throws;

            void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const;

        private:
            Object<Type>& operator=(const Object& cloneFrom);
        };

        std::vector<Object<NascentRawGeometry>>                         _geos;
        std::vector<Object<NascentModelCommandStream>>                  _models;
        std::vector<Object<UnboundSkinController>>                      _skinControllers;
        std::vector<Object<UnboundSkinControllerAndAttachedSkeleton>>   _skinControllersAndSkeletons;
        std::vector<Object<UnboundMorphController>>                     _morphControllers;
        std::vector<Object<NascentBoundSkinnedGeometry>>                _boundSkinnedGeometry;
        std::vector<Object<Assets::RawAnimationCurve>>                  _animationCurves;
        std::vector<Object<Assets::RawMaterial>>                        _materials;
        std::vector<Object<ReferencedTexture>>                          _referencedTextures;
        std::vector<Object<ReferencedMaterial>>                         _referencedMaterials;

        template <typename Type> const std::vector<Object<Type>>&   GetSet() const;
        template <typename Type> std::vector<Object<Type>>&         GetSet();
    };

}}

