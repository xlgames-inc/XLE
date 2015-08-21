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

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    RenderStateSet::RenderStateSet()
    {
        _doubleSided = false;
        _wireframe = false;
        _writeMask = 0xf;
        _deferredBlend = DeferredBlend::Opaque;
        _depthBias = 0;
        _flag = 0;
        
        _forwardBlendSrc = Metal::Blend::One;
        _forwardBlendDst = Metal::Blend::Zero;
        _forwardBlendOp = Metal::BlendOp::NoBlending;
    }

    uint64 RenderStateSet::GetHash() const
    {
        static_assert(sizeof(*this) == sizeof(uint64), "expecting StateSet to be 64 bits long");
        return *(const uint64*)this;
    }

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

    static bool Is(const InputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        const auto* a = section._start;
        const auto* b = match;
        for (;;) {
            if (a == section._end)
                return !(*b);   // success if both strings have terminated at the same time
            if (*b != *a) return false;
            assert(*b); // potentially hit this assert if there are null characters in "section"... that isn't supported
            ++b; ++a;
        }
    }

    static bool IsI(const InputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        const auto* a = section._start;
        const auto* b = match;
        for (;;) {
            if (a == section._end)
                return !(*b);   // success if both strings have terminated at the same time
            if (XlToLower(*b) != XlToLower(*a)) return false;
            assert(*b); // potentially hit this assert if there are null characters in "section"... that isn't supported
            ++b; ++a;
        }
    }

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
                auto value = child.RawValue();
                for (unsigned c=0; c<dimof(s_blendNames); ++c)
                    if (!IsI(value, s_blendNames[c].second))
                        return s_blendNames[c].first;
                return (Metal::Blend::Enum)XlAtoI32((const char*)child.Value().c_str());
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
                auto value = child.RawValue();
                for (unsigned c=0; c<dimof(s_blendOpNames); ++c)
                    if (!IsI(value, s_blendOpNames[c].second))
                        return s_blendOpNames[c].first;
                return (Metal::BlendOp::Enum)XlAtoI32((const char*)child.Value().c_str());
            }
        }

        return Metal::BlendOp::NoBlending;
    }

    static RenderStateSet DeserializeStateSet(InputStreamFormatter<utf8>& formatter)
    {
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
            auto child = doc.Attribute(u("DeferredBlend"));
            if (child) {
                if (IsI(child.RawValue(), u("decal"))) {
                    result._deferredBlend = RenderStateSet::DeferredBlend::Decal;
                } else {
                    result._deferredBlend = RenderStateSet::DeferredBlend::Opaque;
                }
                result._flag |= RenderStateSet::Flag::DeferredBlend;
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

    static const utf8* AsString(RenderStateSet::DeferredBlend::Enum blend)
    {
        switch (blend) {
        case RenderStateSet::DeferredBlend::Decal: return u("decal");
        default:
        case RenderStateSet::DeferredBlend::Opaque: return u("opaque");
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

    void SerializeStateSet(OutputStreamFormatter& formatter, const RenderStateSet& stateSet)
    {
        if (stateSet._flag & RenderStateSet::Flag::DoubleSided)
            formatter.WriteAttribute(u("DoubleSided"), AutoAsString(stateSet._doubleSided));

        if (stateSet._flag & RenderStateSet::Flag::Wireframe)
            formatter.WriteAttribute(u("Wireframe"), AutoAsString(stateSet._wireframe));

        if (stateSet._flag & RenderStateSet::Flag::WriteMask)
            formatter.WriteAttribute(u("WriteMask"), AutoAsString(stateSet._writeMask));

        if (stateSet._flag & RenderStateSet::Flag::DeferredBlend)
            formatter.WriteAttribute(u("DeferredBlend"), AsString(stateSet._deferredBlend));

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

    static Assets::RenderStateSet Merge(
        Assets::RenderStateSet underride,
        Assets::RenderStateSet override)
    {
        typedef Assets::RenderStateSet StateSet;
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
        if (override._flag & StateSet::Flag::DeferredBlend) {
            result._deferredBlend = override._deferredBlend;
            result._flag |= StateSet::Flag::DeferredBlend;
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

    void MakeConcreteRawMaterialFilename(::Assets::ResChar dest[], unsigned dstCount, const ::Assets::ResChar inputName[])
    {
        if (dest != inputName) {
            XlCopyString(dest, dstCount, inputName);
        }

            //  If we're attempting to load from a .dae file, then we need to
            //  instead redirect the query towards the compiled version
        auto ext = XlExtension(dest);
        if (!XlFindStringI(dest, "-rawmat") && (!ext || !XlCompareStringI(ext, "dae"))) {
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            XlChopExtension(dest);
            XlCatString(dest, dstCount, "-rawmat");
            store.MakeIntermediateName(dest, dstCount, dest);
        }
    }

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

    RawMaterial::RawMatSplitName::RawMatSplitName() {}

    RawMaterial::RawMatSplitName::RawMatSplitName(const ::Assets::ResChar initialiser[])
    {
            // We're expecting an initialiser of the format "filename:setting"
        auto colon = XlFindCharReverse(initialiser, ':');
        if (!colon)
            Throw(::Assets::Exceptions::InvalidAsset(initialiser, ""));

        ::Assets::ResChar rawFilename[MaxPath];
        XlCopyNString(rawFilename, initialiser, colon - initialiser);

        ::Assets::ResChar concreteFilename[MaxPath];
        MakeConcreteRawMaterialFilename(concreteFilename, dimof(concreteFilename), rawFilename);

        _initializerName = ::Assets::rstring(concreteFilename) + colon;

        _settingName = colon+1;
        _concreteFilename  = concreteFilename;
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
                    if (Is(eleName, u("Inherit"))) {
                        _inherit = DeserializeInheritList(formatter);
                    } else if (Is(eleName, u("ShaderParams"))) {
                        _matParamBox = ParameterBox(formatter);
                    } else if (Is(eleName, u("Constants"))) {
                        _constants = ParameterBox(formatter);
                    } else if (Is(eleName, u("ResourceBindings"))) {
                        _resourceBindings = ParameterBox(formatter);
                    } else if (Is(eleName, u("States"))) {
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

        {
            auto ele = formatter.BeginElement(u("ShaderParams"));
            _matParamBox.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        {
            auto ele = formatter.BeginElement(u("Constants"));
            _constants.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        {
            auto ele = formatter.BeginElement(u("ResourceBindings"));
            _resourceBindings.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        {
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

    static void AddDep(std::vector<::Assets::DependentFileState>& deps, const RawMaterial::RawMatSplitName& splitName)
    {
            // we need to call "GetDependentFileState" first, because this can change the
            // format of the filename. String compares alone aren't working well for us here
        auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
        auto depState = store.GetDependentFileState(splitName._concreteFilename.c_str());

        auto existing = std::find_if(deps.cbegin(), deps.cend(),
            [&](const ::Assets::DependentFileState& test) 
            {
                return !XlCompareStringI(test._filename.c_str(), depState._filename.c_str());
            });
        if (existing == deps.cend()) {
            deps.push_back(depState);
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
            RawMaterial::RawMatSplitName splitName(i->c_str());
            
                // we still need to add a dependency, even if it's a missing file
            if (deps) AddDep(*deps, splitName);

            TRY {
                auto& rawParams = ::Assets::GetAssetDep<
                    ::Assets::ConfigFileListContainer<RawMaterial>>(
                        splitName._initializerName.c_str());

                ::Assets::DirectorySearchRules newSearchRules = searchRules;
                newSearchRules.Merge(rawParams._searchRules);

                rawParams._asset.Resolve(result, newSearchRules, deps);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END
        }

        MergeInto(result);
    }



}}

