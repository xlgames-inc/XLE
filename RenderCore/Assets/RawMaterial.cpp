// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RawMaterial.h"
#include "Material.h"
#include "../Types.h"
#if defined(HAS_XLE_FULLASSETS)
    #include "../../Assets/Assets.h"
    #include "../../Assets/IntermediateAssets.h"
    #include "../../Assets/DeferredConstruction.h"
#endif
// #include "../../Assets/ConfigFileContainer.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/StringFormat.h"

#if defined(HAS_XLE_FULLASSETS)
namespace Assets
{
    template<> uint64 GetCompileProcessType<::RenderCore::Assets::RawMaterial>() 
        { return ConstHash64<'RawM', 'at'>::Value; }
}
#endif

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const std::pair<Blend, const utf8*> s_blendNames[] =
    {
        std::make_pair(Blend::Zero, u("zero")),
        std::make_pair(Blend::One, u("one")),
            
        std::make_pair(Blend::SrcColor, u("srccolor")),
        std::make_pair(Blend::InvSrcColor, u("invsrccolor")),
        std::make_pair(Blend::DestColor, u("destcolor")),
        std::make_pair(Blend::InvDestColor, u("invdestcolor")),

        std::make_pair(Blend::SrcAlpha, u("srcalpha")),
        std::make_pair(Blend::InvSrcAlpha, u("invsrcalpha")),
        std::make_pair(Blend::DestAlpha, u("destalpha")),
        std::make_pair(Blend::InvDestAlpha, u("invdestalpha")),
    };

    static const std::pair<BlendOp, const utf8*> s_blendOpNames[] =
    {
        std::make_pair(BlendOp::NoBlending, u("noblending")),
        std::make_pair(BlendOp::NoBlending, u("none")),
        std::make_pair(BlendOp::NoBlending, u("false")),

        std::make_pair(BlendOp::Add, u("add")),
        std::make_pair(BlendOp::Subtract, u("subtract")),
        std::make_pair(BlendOp::RevSubtract, u("revSubtract")),
        std::make_pair(BlendOp::Min, u("min")),
        std::make_pair(BlendOp::Max, u("max"))
    };
    
    static Blend DeserializeBlend(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendNames); ++c)
                    if (XlEqStringI(value, s_blendNames[c].second))
                        return s_blendNames[c].first;
                return (Blend)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return Blend::Zero;
    }

    static BlendOp DeserializeBlendOp(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendOpNames); ++c)
                    if (XlEqStringI(value, s_blendOpNames[c].second))
                        return s_blendOpNames[c].first;
                return (BlendOp)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return BlendOp::NoBlending;
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

    static const utf8* AsString(Blend input)
    {
        for (unsigned c=0; c<dimof(s_blendNames); ++c) {
            if (s_blendNames[c].first == input) {
                return s_blendNames[c].second;
            }
        }
        return u("one");
    }

    static const utf8* AsString(BlendOp input)
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
        const ::Assets::DirectorySearchRules& searchRules, StringSection<char> baseMatName)
    {
        if (baseMatName.begin() != resolvedFile)
            XlCopyString(resolvedFile, resolvedFileCount, baseMatName);
        if (!XlExtension(resolvedFile))
            XlCatString(resolvedFile, resolvedFileCount, ".material");
        searchRules.ResolveFile(resolvedFile, resolvedFileCount, resolvedFile);
    }

    uint64 MakeMaterialGuid(StringSection<utf8> name)
    {
            //  If the material name is just a number, then we will use that
            //  as the guid. Otherwise we hash the name.
        const char* parseEnd = nullptr;
        uint64 hashId = XlAtoI64((const char*)name.begin(), &parseEnd, 16);
        if (!parseEnd || parseEnd != (const char*)name.end()) { hashId = Hash64(name.begin(), name.end()); }
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
            default:
                assert(0);
            }
        }
    }

    RawMaterial::RawMaterial(
		InputStreamFormatter<utf8>& formatter, 
		const ::Assets::DirectorySearchRules& searchRules, 
		const std::shared_ptr<::Assets::DependencyValidation>& depVal)
	: _depVal(depVal), _searchRules(searchRules)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    if (XlEqString(name, u("TechniqueConfig"))) {
                        _techniqueConfig = Conversion::Convert<AssetName>(value.AsString());
                    } 
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
            default:
                assert(0);
            }
        }
    }

    RawMaterial::~RawMaterial() {}

    void RawMaterial::Serialize(OutputStreamFormatter& formatter) const
    {
        if (!_techniqueConfig.empty())
            formatter.WriteAttribute(u("TechniqueConfig"), Conversion::Convert<std::basic_string<utf8>>(_techniqueConfig));

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

        if (!_techniqueConfig.empty()) {
            // Shader names in "ResolvedMaterial" are kept very short. We
            // want the material object to be fairly light-weight. 
            if (_techniqueConfig.size() > (dimof(dest._techniqueConfig)-1))
                Throw(::Exceptions::BasicLabel("Shader name is too long during resolve"));

            XlCopyString(dest._techniqueConfig, _techniqueConfig);
        }
    }

    auto RawMaterial::ResolveInherited(
        const ::Assets::DirectorySearchRules& searchRules) const -> std::vector<AssetName>
    {
        std::vector<AssetName> result;

        for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
            auto name = *i;

            auto* colon = XlFindCharReverse(name.c_str(), ':');
            if (colon) {
                ::Assets::ResChar resolvedFile[MaxPath];
                XlCopyNString(resolvedFile, name.c_str(), colon-name.c_str());
                ResolveMaterialFilename(resolvedFile, dimof(resolvedFile), searchRules, resolvedFile);
                
                StringMeld<MaxPath, ::Assets::ResChar> finalRawMatName;
                finalRawMatName << resolvedFile << colon;
                result.push_back((AssetName)finalRawMatName);
            } else {
                result.push_back(name);
            }
        }

        return result;
    }

#if defined(HAS_XLE_FULLASSETS)
    static void AddDep(
        std::vector<::Assets::DependentFileState>& deps, 
        StringSection<::Assets::ResChar> filename)
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
#endif

    static bool IsMaterialFile(StringSection<::Assets::ResChar> extension) { return XlEqStringI(extension, "material"); }
    
    auto RawMaterial::GetAsset(StringSection<::Assets::ResChar> initializer) -> const RawMaterial& 
    {
#if defined(HAS_XLE_FULLASSETS)
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
            return ::Assets::GetAssetComp<RawMaterial>(initializer);
        } else {
            return ::Assets::GetAsset<RawMaterial>(initializer);
        }
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
        return *(const RawMaterial*)nullptr;
#pragma GCC diagnostic pop
#endif
    }

#if defined(HAS_XLE_FULLASSETS)
    auto RawMaterial::GetDivergentAsset(StringSection<::Assets::ResChar> initializer)
        -> const std::shared_ptr<::Assets::DivergentAsset<RawMaterial>>&
    {

        if (!IsMaterialFile(FileNameSplitter<::Assets::ResChar>(initializer).Extension())) {
            return ::Assets::GetDivergentAssetComp<RawMaterial>(initializer);
        } else {
            return ::Assets::GetDivergentAsset<RawMaterial>(initializer);
        }
    }
#endif

	std::unique_ptr<RawMaterial> RawMaterial::CreateNew(StringSection<::Assets::ResChar> initialiser)
	{
		return std::make_unique<RawMaterial>();
	}

    ::Assets::AssetState RawMaterial::TryResolve(
        ResolvedMaterial& result,
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<::Assets::DependentFileState>* deps) const
    {

            // resolve all of the inheritance options and generate a final 
            // ResolvedMaterial object. We need to start at the bottom of the
            // inheritance tree, and merge in new parameters as we come across them.

		::Assets::DirectorySearchRules newSearchRules = _searchRules;
		newSearchRules.Merge(searchRules);

        auto inheritted = ResolveInherited(searchRules);
        for (auto i=inheritted.cbegin(); i!=inheritted.cend(); ++i) {
            FileNameSplitter<::Assets::ResChar> splitName(i->c_str());
            
#if defined(HAS_XLE_FULLASSETS)
                // we still need to add a dependency, even if it's a missing file
            if (deps) AddDep(*deps, splitName.FullFilename());
#endif

            auto& rawParams = GetAsset(i->c_str());
            auto state = rawParams.TryResolve(result, newSearchRules, deps);
			if (state == ::Assets::AssetState::Pending)
				return ::Assets::AssetState::Pending;
        }

        MergeInto(result);
		return ::Assets::AssetState::Ready;
    }

#if defined(HAS_XLE_FULLASSETS)
	std::shared_ptr<::Assets::DeferredConstruction> RawMaterial::BeginDeferredConstruction(
		const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
	{
		return ::Assets::DefaultBeginDeferredConstruction<RawMaterial>(initializers, initializerCount);
	}
#endif

}}

