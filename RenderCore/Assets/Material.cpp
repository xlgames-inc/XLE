// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Material.h"
#include "../Metal/State.h"                 // (just for Blend/BlendOp enum members... maybe we need a higher level version of these enums?)
#include "../../Assets/AssetUtils.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/InvalidAssetManager.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/PathUtils.h"

namespace Assets
{
    template<> uint64 GetCompileProcessType<::Assets::ConfigFileListContainer<RenderCore::Assets::RawMaterial>>() 
        { return ConstHash64<'RawM', 'at'>::Value; }
}

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    ResolvedMaterial::ResolvedMaterial() {}

    ResolvedMaterial::ResolvedMaterial(ResolvedMaterial&& moveFrom)
    : _bindings(std::move(moveFrom._bindings))
    , _matParams(std::move(moveFrom._matParams))
    , _stateSet(moveFrom._stateSet)
    , _constants(std::move(moveFrom._constants))
    {}

    ResolvedMaterial& ResolvedMaterial::operator=(ResolvedMaterial&& moveFrom)
    {
        _bindings = std::move(moveFrom._bindings);
        _matParams = std::move(moveFrom._matParams);
        _stateSet = moveFrom._stateSet;
        _constants = std::move(moveFrom._constants);
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const std::pair<Metal::Blend::Enum, const utf8*> s_blendNames[] =
    {
        std::make_pair(Metal::Blend::Zero, u("zero")),
        std::make_pair(Metal::Blend::One, u("one")),
            
        std::make_pair(Metal::Blend::SrcColor, u("srccolor")),
        std::make_pair(Metal::Blend::InvSrcColor, u("invsrccolor")),
        std::make_pair(Metal::Blend::DestColor, u("destcolor")),
        std::make_pair(Metal::Blend::InvDestColor, u("invdestcolor")),

        std::make_pair(Metal::Blend::SrcAlpha, u("srcalpha")),
        std::make_pair(Metal::Blend::InvSrcAlpha, u("invsrcalpha")),
        std::make_pair(Metal::Blend::DestAlpha, u("destalpha")),
        std::make_pair(Metal::Blend::InvDestAlpha, u("invdestalpha")),
    };

    static const std::pair<Metal::BlendOp::Enum, const utf8*> s_blendOpNames[] =
    {
        std::make_pair(Metal::BlendOp::NoBlending, u("noblending")),
        std::make_pair(Metal::BlendOp::NoBlending, u("none")),
        std::make_pair(Metal::BlendOp::NoBlending, u("false")),

        std::make_pair(Metal::BlendOp::Add, u("add")),
        std::make_pair(Metal::BlendOp::Subtract, u("subtract")),
        std::make_pair(Metal::BlendOp::RevSubtract, u("revSubtract")),
        std::make_pair(Metal::BlendOp::Min, u("min")),
        std::make_pair(Metal::BlendOp::Max, u("max"))
    };
    
    static Metal::Blend::Enum DeserializeBlend(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendNames); ++c)
                    if (XlEqStringI(value, s_blendNames[c].second))
                        return s_blendNames[c].first;
                return (Metal::Blend::Enum)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return Metal::Blend::Zero;
    }

    static Metal::BlendOp::Enum DeserializeBlendOp(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendOpNames); ++c)
                    if (XlEqStringI(value, s_blendOpNames[c].second))
                        return s_blendOpNames[c].first;
                return (Metal::BlendOp::Enum)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return Metal::BlendOp::NoBlending;
    }

    static Techniques::RenderStateSet DeserializeStateSet(InputStreamFormatter<utf8>& formatter)
    {
        using RenderStateSet = Techniques::RenderStateSet;
        RenderStateSet result;

        Document<InputStreamFormatter<utf8>> doc(formatter);

        {
            auto child = doc.Attribute(u("DoubleSided")).As<bool>();
            if (child.first) {
                result._doubleSided = child.second;
                result._flag |= RenderStateSet::Flag::DoubleSided;
            }
        }
        {
            auto child = doc.Attribute(u("Wireframe")).As<bool>();
            if (child.first) {
                result._wireframe = child.second;
                result._flag |= RenderStateSet::Flag::Wireframe;
            }
        }
        {
            auto child = doc.Attribute(u("WriteMask")).As<unsigned>();
            if (child.first) {
                result._writeMask = child.second;
                result._flag |= RenderStateSet::Flag::WriteMask;
            }
        }
        {
            auto child = doc.Attribute(u("BlendType"));
            if (child) {
                if (XlEqStringI(child.Value(), u("decal"))) {
                    result._blendType = RenderStateSet::BlendType::DeferredDecal;
                } else if (XlEqStringI(child.Value(), u("ordered"))) {
                    result._blendType = RenderStateSet::BlendType::Ordered;
                } else {
                    result._blendType = RenderStateSet::BlendType::Basic;
                }
                result._flag |= RenderStateSet::Flag::BlendType;
            }
        }
        {
            auto child = doc.Attribute(u("DepthBias")).As<int>();
            if (child.first) {
                result._depthBias = child.second;
                result._flag |= RenderStateSet::Flag::DepthBias;
            }
        }
        {
            auto child = doc.Element(u("ForwardBlend"));
            if (child) {
                result._forwardBlendSrc = DeserializeBlend(child, u("Src"));
                result._forwardBlendDst = DeserializeBlend(child, u("Dst"));
                result._forwardBlendOp = DeserializeBlendOp(child, u("Op"));
                result._flag |= RenderStateSet::Flag::ForwardBlend;
            }
        }
        return result;
    }

    static const utf8* AsString(Techniques::RenderStateSet::BlendType blend)
    {
        switch (blend) {
        case Techniques::RenderStateSet::BlendType::DeferredDecal: return u("decal");
        case Techniques::RenderStateSet::BlendType::Ordered: return u("ordered");
        default:
        case Techniques::RenderStateSet::BlendType::Basic: return u("basic");
        }
    }

    static const utf8* AsString(Metal::Blend::Enum input)
    {
        for (unsigned c=0; c<dimof(s_blendNames); ++c) {
            if (s_blendNames[c].first == input) {
                return s_blendNames[c].second;
            }
        }
        return u("one");
    }

    static const utf8* AsString(Metal::BlendOp::Enum input)
    {
        for (unsigned c=0; c<dimof(s_blendOpNames); ++c) {
            if (s_blendOpNames[c].first == input) {
                return s_blendOpNames[c].second;
            }
        }
        return u("noblending");
    }

    template<typename Type>
        std::basic_string<utf8> AutoAsString(const Type& type)
        {
            return Conversion::Convert<std::basic_string<utf8>>(
                ImpliedTyping::AsString(type, true));
        }

    static bool HasSomethingToSerialize(const Techniques::RenderStateSet& stateSet)
    {
        return stateSet._flag != 0;
    }

    static void SerializeStateSet(OutputStreamFormatter& formatter, const Techniques::RenderStateSet& stateSet)
    {
        using RenderStateSet = Techniques::RenderStateSet;
        if (stateSet._flag & RenderStateSet::Flag::DoubleSided)
            formatter.WriteAttribute(u("DoubleSided"), AutoAsString(stateSet._doubleSided));

        if (stateSet._flag & RenderStateSet::Flag::Wireframe)
            formatter.WriteAttribute(u("Wireframe"), AutoAsString(stateSet._wireframe));

        if (stateSet._flag & RenderStateSet::Flag::WriteMask)
            formatter.WriteAttribute(u("WriteMask"), AutoAsString(stateSet._writeMask));

        if (stateSet._flag & RenderStateSet::Flag::BlendType)
            formatter.WriteAttribute(u("BlendType"), AsString(stateSet._blendType));

        if (stateSet._flag & RenderStateSet::Flag::DepthBias)
            formatter.WriteAttribute(u("DepthBias"), AutoAsString(stateSet._depthBias));

        if (stateSet._flag & RenderStateSet::Flag::ForwardBlend) {
            auto ele = formatter.BeginElement(u("ForwardBlend"));
            formatter.WriteAttribute(u("Src"), AsString(stateSet._forwardBlendSrc));
            formatter.WriteAttribute(u("Dst"), AsString(stateSet._forwardBlendDst));
            formatter.WriteAttribute(u("Op"), AsString(stateSet._forwardBlendOp));
            formatter.EndElement(ele);
        }
    }

    static Techniques::RenderStateSet Merge(
        Techniques::RenderStateSet underride,
        Techniques::RenderStateSet override)
    {
        using StateSet = Techniques::RenderStateSet;
        StateSet result = underride;
        if (override._flag & StateSet::Flag::DoubleSided) {
            result._doubleSided = override._doubleSided;
            result._flag |= StateSet::Flag::DoubleSided;
        }
        if (override._flag & StateSet::Flag::Wireframe) {
            result._wireframe = override._wireframe;
            result._flag |= StateSet::Flag::Wireframe;
        }
        if (override._flag & StateSet::Flag::WriteMask) {
            result._writeMask = override._writeMask;
            result._flag |= StateSet::Flag::WriteMask;
        }
        if (override._flag & StateSet::Flag::BlendType) {
            result._blendType = override._blendType;
            result._flag |= StateSet::Flag::BlendType;
        }
        if (override._flag & StateSet::Flag::ForwardBlend) {
            result._forwardBlendSrc = override._forwardBlendSrc;
            result._forwardBlendDst = override._forwardBlendDst;
            result._forwardBlendOp = override._forwardBlendOp;
            result._flag |= StateSet::Flag::ForwardBlend;
        }
        if (override._flag & StateSet::Flag::DepthBias) {
            result._depthBias = override._depthBias;
            result._flag |= StateSet::Flag::DepthBias;
        }
        return result;
    }

    RawMaterial::RawMaterial() {}

    void ResolveMaterialFilename(
        ::Assets::ResChar resolvedFile[], unsigned resolvedFileCount,
        const ::Assets::DirectorySearchRules& searchRules, const char baseMatName[])
    {
        if (baseMatName != resolvedFile)
            XlCopyString(resolvedFile, resolvedFileCount, baseMatName);
        if (!XlExtension(resolvedFile))
            XlCatString(resolvedFile, resolvedFileCount, ".material");
        searchRules.ResolveFile(resolvedFile, resolvedFileCount, resolvedFile);
    }

    uint64 MakeMaterialGuid(const utf8* nameStart, const utf8* nameEnd)
    {
            //  If the material name is just a number, then we will use that
            //  as the guid. Otherwise we hash the name.
        const char* parseEnd = nullptr;
        uint64 hashId = XlAtoI64((const char*)nameStart, &parseEnd, 16);
        if (!parseEnd || parseEnd != (const char*)nameEnd) { hashId = Hash64(nameStart, nameEnd); }
        return hashId;
    }

    std::vector<::Assets::rstring> 
        DeserializeInheritList(InputStreamFormatter<utf8>& formatter)
    {
        std::vector<::Assets::rstring> result;

        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    result.push_back(
                        Conversion::Convert<::Assets::rstring>(std::basic_string<utf8>(name._start, name._end)));
                    break;
                }

            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    break;
                }

            case Blob::EndElement:
            case Blob::None:
                return result;
            }
        }
    }

    RawMaterial::RawMaterial(InputStreamFormatter<utf8>& formatter, const ::Assets::DirectorySearchRules&)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    break;
                }

            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                       // first, load inherited settings.
                    if (XlEqString(eleName, u("Inherit"))) {
                        _inherit = DeserializeInheritList(formatter);
                    } else if (XlEqString(eleName, u("ShaderParams"))) {
                        _matParamBox = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("Constants"))) {
                        _constants = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("ResourceBindings"))) {
                        _resourceBindings = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("States"))) {
                        _stateSet = DeserializeStateSet(formatter);
                    } else {
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    break;
                }

            case Blob::EndElement:
            case Blob::None:
                return;
            }
        }
    }

    RawMaterial::~RawMaterial() {}

    void RawMaterial::Serialize(OutputStreamFormatter& formatter) const
    {
        if (!_inherit.empty()) {
            auto ele = formatter.BeginElement(u("Inherit"));
            for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
                auto str = Conversion::Convert<std::basic_string<utf8>>(*i);
                formatter.WriteAttribute(
                    AsPointer(str.cbegin()), AsPointer(str.cend()),
                    (const utf8*)nullptr, (const utf8*)nullptr);
            }
            formatter.EndElement(ele);
        }

        if (_matParamBox.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("ShaderParams"));
            _matParamBox.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (_constants.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("Constants"));
            _constants.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (_resourceBindings.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("ResourceBindings"));
            _resourceBindings.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (HasSomethingToSerialize(_stateSet)) {
            auto ele = formatter.BeginElement(u("States"));
            SerializeStateSet(formatter, _stateSet);
            formatter.EndElement(ele);
        }
    }

    void RawMaterial::MergeInto(ResolvedMaterial& dest) const
    {
        dest._matParams.MergeIn(_matParamBox);
        dest._stateSet = Merge(dest._stateSet, _stateSet);
        dest._constants.MergeIn(_constants);
        dest._bindings.MergeIn(_resourceBindings);
    }

    auto RawMaterial::ResolveInherited(
        const ::Assets::DirectorySearchRules& searchRules) const -> std::vector<ResString>
    {
        std::vector<ResString> result;

        for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
            auto name = *i;

            auto* colon = XlFindCharReverse(name.c_str(), ':');
            if (colon) {
                ::Assets::ResChar resolvedFile[MaxPath];
                XlCopyNString(resolvedFile, name.c_str(), colon-name.c_str());
                ResolveMaterialFilename(resolvedFile, dimof(resolvedFile), searchRules, resolvedFile);
                
                StringMeld<MaxPath, ::Assets::ResChar> finalRawMatName;
                finalRawMatName << resolvedFile << colon;
                result.push_back((ResString)finalRawMatName);
            } else {
                result.push_back(name);
            }
        }

        return result;
    }

    static void AddDep(
        std::vector<::Assets::DependentFileState>& deps, 
        const StringSection<::Assets::ResChar> filename)
    {
            // we need to call "GetDependentFileState" first, because this can change the
            // format of the filename. String compares alone aren't working well for us here
        auto depState = ::Assets::IntermediateAssets::Store::GetDependentFileState(filename);
        auto existing = std::find_if(deps.cbegin(), deps.cend(),
            [&](const ::Assets::DependentFileState& test) 
            {
                return !XlCompareStringI(test._filename.c_str(), depState._filename.c_str());
            });
        if (existing == deps.cend())
            deps.push_back(depState);
    }

    static bool IsMaterialFile(StringSection<::Assets::ResChar> extension) { return XlEqStringI(extension, "material"); }
    
    auto RawMaterial::GetAsset(const ::Assets::ResChar initializer[]) -> const Container& 
    {
        // There are actually 2 paths here... Normally the requested file is a
        // .material file -- in which case we can load it with a  
        // ::Assets::ConfigFileListContainer.
        //
        // However, it could alternatively be a model file. For a model file, we
        // invoke the model compiler to create the "rawmat" file... And then we 
        // can load the rawmat file using ::Assets::ConfigFileListContainer.
        //
        // How can we tell which one it is...? Well, it would be nice if there was
        // some good way to do this -- but all we can do is check the extension
        // currently.
        //
        // Note that when loading from a model file, we can throw "pending". But
        // loading from a .material file should never be pending

        if (!IsMaterialFile(FileNameSplitter<::Assets::ResChar>(initializer).Extension())) {
            return ::Assets::GetAssetComp<Container>(initializer);
        } else {
            return ::Assets::GetAssetDep<Container>(initializer);
        }
    }

    auto RawMaterial::GetDivergentAsset(const ::Assets::ResChar initializer[])
        -> const std::shared_ptr<::Assets::DivergentAsset<Container>>&
    {
        if (!IsMaterialFile(FileNameSplitter<::Assets::ResChar>(initializer).Extension())) {
            return ::Assets::GetDivergentAssetComp<Container>(initializer);
        } else {
            return ::Assets::GetDivergentAsset<Container>(initializer);
        }
    }

    void RawMaterial::Resolve(
        ResolvedMaterial& result,
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<::Assets::DependentFileState>* deps) const
    {
            // resolve all of the inheritance options and generate a final 
            // ResolvedMaterial object. We need to start at the bottom of the
            // inheritance tree, and merge in new parameters as we come across them.

        auto inheritted = ResolveInherited(searchRules);
        for (auto i=inheritted.cbegin(); i!=inheritted.cend(); ++i) {
            FileNameSplitter<::Assets::ResChar> splitName(i->c_str());
            
                // we still need to add a dependency, even if it's a missing file
            if (deps) AddDep(*deps, splitName.FullFilename());

            TRY {
                auto& rawParams = GetAsset(i->c_str());

                ::Assets::DirectorySearchRules newSearchRules = searchRules;
                newSearchRules.Merge(rawParams._searchRules);

                rawParams._asset.Resolve(result, newSearchRules, deps);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END
        }

        MergeInto(result);
    }



}}

