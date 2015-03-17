// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TableOfObjects.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Assets/MaterialSettingsFile.h"
#include "ModelCommandStream.h"
#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/Log.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWUniqueId.h>
#pragma warning(pop)

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    template <typename Type>
        ObjectId    TableOfObjects::Get(const COLLADAFW::UniqueId& id) const
    {
        auto& set = GetSet<Type>();
        for (auto i=set.cbegin(); i!=set.cend(); ++i)
            if (i->_hashId == id)
                return (ObjectId)std::distance(set.cbegin(), i);
        return ObjectId_Invalid;
    }

    template <typename Type>
        bool        TableOfObjects::Has(const COLLADAFW::UniqueId& id) const
    {
        auto& set = GetSet<Type>();
        for (auto i=set.begin(); i!=set.end(); ++i)
            if (i->_hashId == id)
                return true;
        return false;
    }

    template <typename Type>
        const Type*        TableOfObjects::GetFromObjectId(ObjectId id) const never_throws
    {
        auto& set = GetSet<Type>();
        if (id < set.size()) {
            return &set[id]._internalType;
        }
        return nullptr;
    }

    template <> auto     TableOfObjects::GetSet<NascentRawGeometry>() const -> const std::vector<Object<NascentRawGeometry>>& { return _geos; }
    template <> auto     TableOfObjects::GetSet<NascentRawGeometry>() -> std::vector<Object<NascentRawGeometry>>&             { return _geos; }

    template <> auto     TableOfObjects::GetSet<NascentModelCommandStream>() const -> const std::vector<Object<NascentModelCommandStream>>& { return _models; }
    template <> auto     TableOfObjects::GetSet<NascentModelCommandStream>() -> std::vector<Object<NascentModelCommandStream>>&             { return _models; }
        
    template <> auto     TableOfObjects::GetSet<UnboundSkinController>() const -> const std::vector<Object<UnboundSkinController>>&         { return _skinControllers; }
    template <> auto     TableOfObjects::GetSet<UnboundSkinController>() -> std::vector<Object<UnboundSkinController>>&                     { return _skinControllers; }

    template <> auto     TableOfObjects::GetSet<UnboundSkinControllerAndAttachedSkeleton>() const -> const std::vector<Object<UnboundSkinControllerAndAttachedSkeleton>>&         { return _skinControllersAndSkeletons; }
    template <> auto     TableOfObjects::GetSet<UnboundSkinControllerAndAttachedSkeleton>() -> std::vector<Object<UnboundSkinControllerAndAttachedSkeleton>>&                     { return _skinControllersAndSkeletons; }

    template <> auto     TableOfObjects::GetSet<UnboundMorphController>() const -> const std::vector<Object<UnboundMorphController>>&         { return _morphControllers; }
    template <> auto     TableOfObjects::GetSet<UnboundMorphController>() -> std::vector<Object<UnboundMorphController>>&                     { return _morphControllers; }

    template <> auto     TableOfObjects::GetSet<NascentBoundSkinnedGeometry>() const -> const std::vector<Object<NascentBoundSkinnedGeometry>>&         { return _boundSkinnedGeometry; }
    template <> auto     TableOfObjects::GetSet<NascentBoundSkinnedGeometry>() -> std::vector<Object<NascentBoundSkinnedGeometry>>&                     { return _boundSkinnedGeometry; }

    template <> auto     TableOfObjects::GetSet<Assets::RawAnimationCurve>() const -> const std::vector<Object<Assets::RawAnimationCurve>>&         { return _animationCurves; }
    template <> auto     TableOfObjects::GetSet<Assets::RawAnimationCurve>() -> std::vector<Object<Assets::RawAnimationCurve>>&                     { return _animationCurves; }

    template <> auto     TableOfObjects::GetSet<Assets::RawMaterial>() const -> const std::vector<Object<Assets::RawMaterial>>&         { return _materials; }
    template <> auto     TableOfObjects::GetSet<Assets::RawMaterial>() -> std::vector<Object<Assets::RawMaterial>>&                     { return _materials; }

    template <> auto     TableOfObjects::GetSet<ReferencedTexture>() const -> const std::vector<Object<ReferencedTexture>>&         { return _referencedTextures; }
    template <> auto     TableOfObjects::GetSet<ReferencedTexture>() -> std::vector<Object<ReferencedTexture>>&                     { return _referencedTextures; }

    template <> auto     TableOfObjects::GetSet<ReferencedMaterial>() const -> const std::vector<Object<ReferencedMaterial>>&         { return _referencedMaterials; }
    template <> auto     TableOfObjects::GetSet<ReferencedMaterial>() -> std::vector<Object<ReferencedMaterial>>&                     { return _referencedMaterials; }

    template <typename Type>
        ObjectId        TableOfObjects::Add(    const std::string& idString, 
                                                const std::string& name,
                                                const COLLADAFW::UniqueId& id,
                                                Type&& object)
    {
        auto& set = GetSet<Type>();
        for (auto i=set.begin(); i!=set.end(); ++i)
            if (i->_hashId == id)
                ThrowException(FormatError("Duplicate object ids found while building table of local objects! (%s)", idString.c_str()));

        Object<Type> newObject(idString, name, id, std::forward<Type>(object));
        set.push_back(std::move(newObject));
        return ObjectId(set.size()-1);
    }

    template <typename Type>
        std::tuple<std::string, std::string, COLLADAFW::UniqueId> 
            TableOfObjects::GetDesc(ObjectId id) const never_throws
    {
        auto& set = GetSet<Type>();
        if (id < set.size()) {
            return std::make_tuple(set[id]._idString, set[id]._name, set[id]._hashId);
        }
        return std::tuple<std::string, std::string, COLLADAFW::UniqueId>();
    }

    template<typename Type>
        TableOfObjects::Object<Type>::Object(const std::string& idString, const std::string& name, const COLLADAFW::UniqueId& hashId, Type&& internalType)
        :   _idString(idString), _name(name), _hashId(hashId), _internalType(std::forward<Type>(internalType))
        {}
    template<typename Type>
        TableOfObjects::Object<Type>::~Object() {}

    template<typename Type>
        TableOfObjects::Object<Type>::Object(Object&& moveFrom) never_throws 
        :   _idString(std::move(moveFrom._idString))
        ,   _name(std::move(moveFrom._name))
        ,   _internalType(std::move(moveFrom._internalType))
        ,   _hashId(moveFrom._hashId)
        {}

    template<typename Type>
        auto TableOfObjects::Object<Type>::operator=(Object&& moveFrom) never_throws -> Object<Type>&
    {
        _idString = std::move(moveFrom._idString);
        _name = std::move(moveFrom._name);
        _internalType = std::move(moveFrom._internalType);
        _hashId = moveFrom._hashId;
        return *this;
    }

    template<typename Type>
        void TableOfObjects::Object<Type>::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        Serialization::Serialize(outputSerializer, _internalType);
    }

    void    TableOfObjects::SerializeSkin(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const
    {
        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = _geos.begin(); i!=_geos.end(); ++i) {
                i->_internalType.Serialize(tempBlock, largeResourcesBlock);
            }
            outputSerializer.SerializeSubBlock(tempBlock);
            outputSerializer.SerializeValue(_geos.size());
        }

        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = _boundSkinnedGeometry.begin(); i!=_boundSkinnedGeometry.end(); ++i) {
                i->_internalType.Serialize(tempBlock, largeResourcesBlock);
            }
            outputSerializer.SerializeSubBlock(tempBlock);
            outputSerializer.SerializeValue(_boundSkinnedGeometry.size());
        }

        // outputSerializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        // outputSerializer.SerializeValue(_materials.size());
    }

    void    TableOfObjects::SerializeAnimationSet(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeSubBlock(AsPointer(_animationCurves.begin()), AsPointer(_animationCurves.end()));
        outputSerializer.SerializeValue(_animationCurves.size());
    }

    TableOfObjects::TableOfObjects(TableOfObjects&& moveFrom)
    :   _geos(std::move(moveFrom._geos))
    ,   _models(std::move(moveFrom._models))
    ,   _skinControllers(std::move(moveFrom._skinControllers))
    ,   _skinControllersAndSkeletons(std::move(moveFrom._skinControllersAndSkeletons))
    ,   _boundSkinnedGeometry(std::move(moveFrom._boundSkinnedGeometry))
    ,   _animationCurves(std::move(moveFrom._animationCurves))
    ,   _materials(std::move(_materials))
    ,   _referencedTextures(std::move(moveFrom._referencedTextures))
    ,   _referencedMaterials(std::move(moveFrom._referencedMaterials))
    {
    }

    TableOfObjects& TableOfObjects::operator=(TableOfObjects&& moveFrom) never_throws
    {
        _geos = std::move(moveFrom._geos);
        _models = std::move(moveFrom._models);
        _skinControllers = std::move(moveFrom._skinControllers);
        _skinControllersAndSkeletons = std::move(moveFrom._skinControllersAndSkeletons);
        _boundSkinnedGeometry = std::move(moveFrom._boundSkinnedGeometry);
        _animationCurves = std::move(moveFrom._animationCurves);
        _materials = std::move(moveFrom._materials);
        _referencedTextures = std::move(moveFrom._referencedTextures);
        _referencedMaterials = std::move(moveFrom._referencedMaterials);
        return *this;
    }

    TableOfObjects::TableOfObjects() {}
    TableOfObjects::~TableOfObjects() {}


    template <typename Type>
        void Instantiator()
    {
        auto i0 = &TableOfObjects::Get<Type>;
        auto i1 = &TableOfObjects::Has<Type>;
        auto i2 = &TableOfObjects::Add<Type>;
        auto i3 = &TableOfObjects::GetDesc<Type>;
        auto i4 = &TableOfObjects::GetFromObjectId<Type>;
        (void)i0; (void)i1; (void)i2; (void)i3; (void)i4;
    }

    template void Instantiator<NascentRawGeometry>();
    template void Instantiator<NascentModelCommandStream>();
    template void Instantiator<UnboundSkinController>();
    template void Instantiator<UnboundSkinControllerAndAttachedSkeleton>();
    template void Instantiator<UnboundMorphController>();
    template void Instantiator<NascentBoundSkinnedGeometry>();
    template void Instantiator<Assets::RawAnimationCurve>();
    template void Instantiator<Assets::RawMaterial>();
    template void Instantiator<ReferencedTexture>();
    template void Instantiator<ReferencedMaterial>();


    void Warning(const char format[], ...)
    {
        char buffer[4096];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, dimof(buffer), format, args);
        va_end(args);

        LogWarning << buffer;
    }

}}


