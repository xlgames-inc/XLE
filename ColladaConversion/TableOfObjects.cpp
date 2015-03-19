// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TableOfObjects.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Assets/Material.h"
#include "ModelCommandStream.h"
#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Data.h"

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
        ObjectId    TableOfObjects::GetObjectId(const COLLADAFW::UniqueId& id) const
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
            Serialization::Serialize(outputSerializer, _geos.size());
        }

        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i = _boundSkinnedGeometry.begin(); i!=_boundSkinnedGeometry.end(); ++i) {
                i->_internalType.Serialize(tempBlock, largeResourcesBlock);
            }
            outputSerializer.SerializeSubBlock(tempBlock);
            Serialization::Serialize(outputSerializer, _boundSkinnedGeometry.size());
        }

        {
            Serialization::NascentBlockSerializer tempBlock;
            for (auto i=_referencedMaterials.cbegin(); i!=_referencedMaterials.cend(); ++i) {
                Serialization::Serialize(tempBlock, i->_internalType._guid);
            }
            outputSerializer.SerializeSubBlock(tempBlock);
            Serialization::Serialize(outputSerializer, _referencedMaterials.size());
        }
    }

    void    TableOfObjects::SerializeAnimationSet(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeSubBlock(AsPointer(_animationCurves.begin()), AsPointer(_animationCurves.end()));
        outputSerializer.SerializeValue(_animationCurves.size());
    }

    std::vector<std::unique_ptr<Data>>  TableOfObjects::SerializeMaterial(const std::string& matSettingsFile) const
    {
        // We're going to write a text file chunk containing all
        // of the raw material settings. We need to do this for every ReferencedMaterial
        // in the table of objects.
        // Note that some of these materials might not actually be used by any meshes...
        // It looks like the max exporter can export redundant materials sometimes.
        // So we could cull down this list. there might be some cases where unreferenced
        // materials might be useful, though...?

        std::vector<std::unique_ptr<Data>> result;
        for (auto i=_referencedMaterials.cbegin(); i!=_referencedMaterials.cend(); ++i) {

            auto objectId = GetObjectId<Assets::RawMaterial>(
                i->_internalType._effectId.AsColladaId());

            if (objectId != ObjectId_Invalid) {
                const auto* obj = GetFromObjectId<Assets::RawMaterial>(objectId);
                if (obj) {
                    Assets::RawMaterial matCopy = *obj;

                        //  We must set the inherit settings here, because we are
                        //  triggering these off the material name, not the effect
                        //  name.
                        //  When we first constructed the RawMaterial object in Writer::writeEffect,
                        //  we don't know what materials will use the effect, and so we
                        //  don't know the material names to use. We could use the effect
                        //  name, I guess. We want to use the name that most closely matches
                        //  the names set in Max -- and that seems to be the material name.
                    matCopy._inherit.push_back(matSettingsFile + ":*");
                    matCopy._inherit.push_back(matSettingsFile + ":" + i->_name.c_str());

                    auto newBlock = matCopy.SerializeAsData();
                    newBlock->SetValue(i->_name.c_str());
                    result.push_back(std::move(newBlock));
                }
            }
        }

        return std::move(result);
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
        auto i0 = &TableOfObjects::GetObjectId<Type>;
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


