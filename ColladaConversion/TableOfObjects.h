// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ConversionObjects.h"
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
        template <typename Type> bool Has(ObjectGuid id) const never_throws;
        template <typename Type> const Type* Get(ObjectGuid id) const never_throws;
        template <typename Type> void Add(ObjectGuid id, const std::string& name, const std::string& idString, Type&& object);

        template <typename Type>
            std::tuple<std::string, std::string> 
                GetDesc(ObjectGuid id) const never_throws;

        void SerializeSkin(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;
        void SerializeAnimationSet(Serialization::NascentBlockSerializer& outputSerializer) const;
        std::vector<std::unique_ptr<Data>> SerializeMaterial(const std::string& matSettingsFile) const;

        TableOfObjects();
        ~TableOfObjects();
        TableOfObjects(TableOfObjects&& moveFrom);
        TableOfObjects& operator=(TableOfObjects&& moveFrom) never_throws;
    private:
        template<typename Type> class Object
        {
        public:
            ObjectGuid      _id;
            std::string     _name;
            std::string     _idString;
            Type            _internalType;

            Object(ObjectGuid id, const std::string& name, const std::string& idString, Type&& internalType);
            ~Object();
            Object(Object&& moveFrom) never_throws;
            Object& operator=(Object&& moveFrom) never_throws;

            void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const;

        private:
            Object(const Object& cloneFrom) = delete;
            Object& operator=(const Object& cloneFrom) = delete;
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

        template <typename Type> const std::vector<Object<Type>>&   GetCollection() const;
        template <typename Type> std::vector<Object<Type>>&         GetCollection();
    };

}}

