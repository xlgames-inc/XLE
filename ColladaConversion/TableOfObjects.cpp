// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TableOfObjects.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"
#include "ModelCommandStream.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Assets/Material.h"
#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/Data.h"

// #pragma warning(push)
// #pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
// #pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
// #pragma warning(disable:4512)       // assignment operator could not be generated
//     #include <COLLADAFWUniqueId.h>
// #pragma warning(pop)

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    ObjectGuid GetGuid(const ObjectGuid& obj) { return obj; }
    template<typename Type> ObjectGuid GetGuid(const Type& t) { return t._id; }

    struct CompareGuid
    {
        template <typename Lhs, typename Rhs>
            bool operator()(const Lhs& lhs, const Rhs& rhs) { return GetGuid(lhs) < GetGuid(rhs); }
    };

    template <typename Type>
        bool        TableOfObjects::Has(ObjectGuid id) const
    {
        auto& set = GetCollection<Type>();
        auto i = std::lower_bound(set.cbegin(), set.cend(), id, CompareGuid());
        return i != set.cend() && i->_id == id;
    }

    template <typename Type>
        const Type*        TableOfObjects::Get(ObjectGuid id) const never_throws
    {
        auto& set = GetCollection<Type>();
        auto i = std::lower_bound(set.cbegin(), set.cend(), id, CompareGuid());
        if (i != set.cend() && i->_id == id) return &i->_internalType;
        return nullptr;
    }

    template <> auto     TableOfObjects::GetCollection<NascentRawGeometry>() const -> const std::vector<Object<NascentRawGeometry>>& { return _geos; }
    template <> auto     TableOfObjects::GetCollection<NascentRawGeometry>() -> std::vector<Object<NascentRawGeometry>>&             { return _geos; }

    template <> auto     TableOfObjects::GetCollection<NascentModelCommandStream>() const -> const std::vector<Object<NascentModelCommandStream>>& { return _models; }
    template <> auto     TableOfObjects::GetCollection<NascentModelCommandStream>() -> std::vector<Object<NascentModelCommandStream>>&             { return _models; }
        
    template <> auto     TableOfObjects::GetCollection<UnboundSkinController>() const -> const std::vector<Object<UnboundSkinController>>&         { return _skinControllers; }
    template <> auto     TableOfObjects::GetCollection<UnboundSkinController>() -> std::vector<Object<UnboundSkinController>>&                     { return _skinControllers; }

    template <> auto     TableOfObjects::GetCollection<UnboundSkinControllerAndAttachedSkeleton>() const -> const std::vector<Object<UnboundSkinControllerAndAttachedSkeleton>>&         { return _skinControllersAndSkeletons; }
    template <> auto     TableOfObjects::GetCollection<UnboundSkinControllerAndAttachedSkeleton>() -> std::vector<Object<UnboundSkinControllerAndAttachedSkeleton>>&                     { return _skinControllersAndSkeletons; }

    template <> auto     TableOfObjects::GetCollection<UnboundMorphController>() const -> const std::vector<Object<UnboundMorphController>>&         { return _morphControllers; }
    template <> auto     TableOfObjects::GetCollection<UnboundMorphController>() -> std::vector<Object<UnboundMorphController>>&                     { return _morphControllers; }

    template <> auto     TableOfObjects::GetCollection<NascentBoundSkinnedGeometry>() const -> const std::vector<Object<NascentBoundSkinnedGeometry>>&         { return _boundSkinnedGeometry; }
    template <> auto     TableOfObjects::GetCollection<NascentBoundSkinnedGeometry>() -> std::vector<Object<NascentBoundSkinnedGeometry>>&                     { return _boundSkinnedGeometry; }

    template <> auto     TableOfObjects::GetCollection<Assets::RawAnimationCurve>() const -> const std::vector<Object<Assets::RawAnimationCurve>>&         { return _animationCurves; }
    template <> auto     TableOfObjects::GetCollection<Assets::RawAnimationCurve>() -> std::vector<Object<Assets::RawAnimationCurve>>&                     { return _animationCurves; }

    template <> auto     TableOfObjects::GetCollection<Assets::RawMaterial>() const -> const std::vector<Object<Assets::RawMaterial>>&         { return _materials; }
    template <> auto     TableOfObjects::GetCollection<Assets::RawMaterial>() -> std::vector<Object<Assets::RawMaterial>>&                     { return _materials; }

    template <> auto     TableOfObjects::GetCollection<ReferencedTexture>() const -> const std::vector<Object<ReferencedTexture>>&         { return _referencedTextures; }
    template <> auto     TableOfObjects::GetCollection<ReferencedTexture>() -> std::vector<Object<ReferencedTexture>>&                     { return _referencedTextures; }

    template <> auto     TableOfObjects::GetCollection<ReferencedMaterial>() const -> const std::vector<Object<ReferencedMaterial>>&         { return _referencedMaterials; }
    template <> auto     TableOfObjects::GetCollection<ReferencedMaterial>() -> std::vector<Object<ReferencedMaterial>>&                     { return _referencedMaterials; }

    template <typename Type>
        void TableOfObjects::Add(
            ObjectGuid          id, 
            const std::string&  name,
            const std::string&  idString,
            Type&&              object)
    {
        auto& c = GetCollection<Type>();

        auto i = std::lower_bound(c.cbegin(), c.cend(), id, CompareGuid());
        if (i != c.cend() && i->_id == id)
            ThrowException(FormatError("Duplicate object ids found while building table of local objects! (%s)", idString.c_str()));

        c.insert(i, Object<Type>(id, name, idString, std::forward<Type>(object)));
    }

    template <typename Type>
        std::tuple<std::string, std::string> 
            TableOfObjects::GetDesc(ObjectGuid id) const never_throws
    {
        auto& c = GetCollection<Type>();
        auto i = std::lower_bound(c.cbegin(), c.cend(), id, CompareGuid());
        if (i != c.cend() && i->_id == id)
            return std::make_tuple(i->_name, i->_idString);
        return std::tuple<std::string, std::string>();
    }

    template<typename Type>
        TableOfObjects::Object<Type>::Object(ObjectGuid id, const std::string& name, const std::string& idString, Type&& internalType)
        :   _idString(idString), _name(name), _id(id), _internalType(std::forward<Type>(internalType))
        {}

    template<typename Type>
        TableOfObjects::Object<Type>::~Object() {}

    template<typename Type>
        TableOfObjects::Object<Type>::Object(Object&& moveFrom) never_throws 
        :   _idString(std::move(moveFrom._idString))
        ,   _name(std::move(moveFrom._name))
        ,   _internalType(std::move(moveFrom._internalType))
        ,   _id(moveFrom._id)
        {}

    template<typename Type>
        auto TableOfObjects::Object<Type>::operator=(Object&& moveFrom) never_throws -> Object<Type>&
    {
        _idString = std::move(moveFrom._idString);
        _name = std::move(moveFrom._name);
        _internalType = std::move(moveFrom._internalType);
        _id = moveFrom._id;
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
            const auto* obj = Get<Assets::RawMaterial>(i->_internalType._effectId);
            if (obj) {
                auto newBlock = obj->SerializeAsData();
                newBlock->SetValue(i->_name.c_str());
                result.push_back(std::move(newBlock));
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
        auto i0 = &TableOfObjects::Get<Type>;
        auto i1 = &TableOfObjects::Has<Type>;
        auto i2 = &TableOfObjects::Add<Type>;
        auto i3 = &TableOfObjects::GetDesc<Type>;
        (void)i0; (void)i1; (void)i2; (void)i3;
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


}}


