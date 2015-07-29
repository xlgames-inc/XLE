// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Material.h"
#include "../Metal/State.h"      // (just for Blend/BlendOp enum members... maybe we need a higher level version of these enums?)
#include "../../Assets/AssetUtils.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/DataSerialize.h"
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

    static const std::pair<Metal::Blend::Enum, const char*> s_blendNames[] =
    {
        std::make_pair(Metal::Blend::Zero, "zero"),
        std::make_pair(Metal::Blend::One, "one"),
            
        std::make_pair(Metal::Blend::SrcColor, "srccolor"),
        std::make_pair(Metal::Blend::InvSrcColor, "invsrccolor"),
        std::make_pair(Metal::Blend::DestColor, "destcolor"),
        std::make_pair(Metal::Blend::InvDestColor, "invdestcolor"),

        std::make_pair(Metal::Blend::SrcAlpha, "srcalpha"),
        std::make_pair(Metal::Blend::InvSrcAlpha, "invsrcalpha"),
        std::make_pair(Metal::Blend::DestAlpha, "destalpha"),
        std::make_pair(Metal::Blend::InvDestAlpha, "invdestalpha"),
    };

    static const std::pair<Metal::BlendOp::Enum, const char*> s_blendOpNames[] =
    {
        std::make_pair(Metal::BlendOp::NoBlending, "noblending"),
        std::make_pair(Metal::BlendOp::NoBlending, "none"),
        std::make_pair(Metal::BlendOp::NoBlending, "false"),

        std::make_pair(Metal::BlendOp::Add, "add"),
        std::make_pair(Metal::BlendOp::Subtract, "subtract"),
        std::make_pair(Metal::BlendOp::RevSubtract, "revSubtract"),
        std::make_pair(Metal::BlendOp::Min, "min"),
        std::make_pair(Metal::BlendOp::Max, "max")
    };
    
    static Metal::Blend::Enum DeserializeBlend(const Data* source, const char name[])
    {
        if (source) {
            auto* child = source->ChildWithValue(name);
            if (child && child->child && child->child->value) {
                const char* value = child->child->value;
                for (unsigned c=0; c<dimof(s_blendNames); ++c) {
                    if (!XlCompareStringI(value, s_blendNames[c].second)) {
                        return s_blendNames[c].first;
                    }
                }
                return (Metal::Blend::Enum)XlAtoI32(value);
            }
        }

        return Metal::Blend::Zero;
    }

    static Metal::BlendOp::Enum DeserializeBlendOp(const Data* source, const char name[])
    {
        if (source) {
            auto* child = source->ChildWithValue(name);
            if (child && child->child && child->child->value) {
                const char* value = child->child->value;
                for (unsigned c=0; c<dimof(s_blendOpNames); ++c) {
                    if (!XlCompareStringI(value, s_blendOpNames[c].second)) {
                        return s_blendOpNames[c].first;
                    }
                }
                return (Metal::BlendOp::Enum)XlAtoI32(value);
            }
        }

        return Metal::BlendOp::NoBlending;
    }

    static RenderStateSet DeserializeStateSet(const Data& src)
    {
        RenderStateSet result;
        {
            auto* child = src.ChildWithValue("DoubleSided");
            if (child && child->child && child->child->value) {
                result._doubleSided = Conversion::Convert<bool>((const char*)child->child->value);
                result._flag |= RenderStateSet::Flag::DoubleSided;
            }
        }
        {
            auto* child = src.ChildWithValue("Wireframe");
            if (child && child->child && child->child->value) {
                result._wireframe = Conversion::Convert<bool>((const char*)child->child->value);
                result._flag |= RenderStateSet::Flag::Wireframe;
            }
        }
        {
            auto* child = src.ChildWithValue("WriteMask");
            if (child && child->child && child->child->value) {
                result._writeMask = child->child->IntValue();
                result._flag |= RenderStateSet::Flag::WriteMask;
            }
        }
        {
            auto* child = src.ChildWithValue("DeferredBlend");
            if (child && child->child && child->child->value) {
                if (XlCompareStringI(child->child->value, "decal")) {
                    result._deferredBlend = RenderStateSet::DeferredBlend::Decal;
                } else {
                    result._deferredBlend = RenderStateSet::DeferredBlend::Opaque;
                }
                result._flag |= RenderStateSet::Flag::DeferredBlend;
            }
        }
        {
            auto* child = src.ChildWithValue("DepthBias");
            if (child && child->child && child->child->value) {
                result._depthBias = child->child->IntValue();
                result._flag |= RenderStateSet::Flag::DepthBias;
            }
        }
        {
            auto* child = src.ChildWithValue("ForwardBlend");
            if (child && child->child) {
                result._forwardBlendSrc = DeserializeBlend(child, "Src");
                result._forwardBlendDst = DeserializeBlend(child, "Dst");
                result._forwardBlendOp = DeserializeBlendOp(child, "Op");
                result._flag |= RenderStateSet::Flag::ForwardBlend;
            }
        }
        return result;
    }

    static const char* AsString(RenderStateSet::DeferredBlend::Enum blend)
    {
        switch (blend) {
        case RenderStateSet::DeferredBlend::Decal: return "decal";
        default:
        case RenderStateSet::DeferredBlend::Opaque: return "opaque";
        }
    }

    static const char* AsString(Metal::Blend::Enum input)
    {
        for (unsigned c=0; c<dimof(s_blendNames); ++c) {
            if (s_blendNames[c].first == input) {
                return s_blendNames[c].second;
            }
        }
        return "one";
    }

    static const char* AsString(Metal::BlendOp::Enum input)
    {
        for (unsigned c=0; c<dimof(s_blendOpNames); ++c) {
            if (s_blendOpNames[c].first == input) {
                return s_blendOpNames[c].second;
            }
        }
        return "noblending";
    }

    std::unique_ptr<Data> SerializeStateSet(const char name[], const RenderStateSet& stateSet)
    {
        // opposite of DeserializeStateSet... create a serialized form of these states
        auto result = std::make_unique<Data>(name);
        if (stateSet._flag & RenderStateSet::Flag::DoubleSided) {
            result->SetAttribute("DoubleSided", stateSet._doubleSided);
        }

        if (stateSet._flag & RenderStateSet::Flag::Wireframe) {
            result->SetAttribute("Wireframe", stateSet._wireframe);
        }

        if (stateSet._flag & RenderStateSet::Flag::WriteMask) {
            result->SetAttribute("WriteMask", stateSet._writeMask);
        }

        if (stateSet._flag & RenderStateSet::Flag::DeferredBlend) {
            result->SetAttribute("DeferredBlend", AsString(stateSet._deferredBlend));
        }

        if (stateSet._flag & RenderStateSet::Flag::DepthBias) {
            result->SetAttribute("DepthBias", stateSet._depthBias);
        }

        if (stateSet._flag & RenderStateSet::Flag::ForwardBlend) {
            auto block = std::make_unique<Data>("ForwardBlend");
            block->SetAttribute("Src", AsString(stateSet._forwardBlendSrc));
            block->SetAttribute("Dst", AsString(stateSet._forwardBlendDst));
            block->SetAttribute("Op", AsString(stateSet._forwardBlendOp));
            result->Add(block.release());
        }

        return std::move(result);
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
        XlNormalizePath(resolvedFile, resolvedFileCount, resolvedFile);
        XlSimplifyPath(resolvedFile, resolvedFileCount, resolvedFile, "\\/");
    }

    uint64 MakeMaterialGuid(const char* nameStart, const char* nameEnd)
    {
            //  If the material name is just a number, then we will use that
            //  as the guid. Otherwise we hash the name.
        const char* parseEnd = nullptr;
        uint64 hashId = XlAtoI64(nameStart, &parseEnd, 16);
        if (!parseEnd || parseEnd != nameEnd) { hashId = Hash64(nameStart, nameEnd); }
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

        _settingName = colon+1;
        _concreteFilename  = concreteFilename;
        _initializerFilename = rawFilename;
    }

    RawMaterial::RawMaterial(const ::Assets::ResChar initialiser[])
    {
        _splitName = RawMatSplitName(initialiser);
        auto searchRules = ::Assets::DefaultDirectorySearchRules(_splitName._initializerFilename.c_str());
        
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(_splitName._concreteFilename.c_str(), &sourceFileSize);
        if (!sourceFile)
            Throw(::Assets::Exceptions::InvalidAsset(initialiser, 
                StringMeld<128>() << "Missing or empty file: " << _splitName._concreteFilename));

        Data data;
        data.Load((const char*)sourceFile.get(), (int)sourceFileSize);

        auto source = data.ChildWithValue(_splitName._settingName.c_str());
        if (!source) {
            StringMeld<64> hashedName;
            hashedName << std::hex << Hash64(_splitName._settingName);
            source = data.ChildWithValue(hashedName);
        }

        if (!source)
            Throw(::Assets::Exceptions::InvalidAsset(initialiser, 
                StringMeld<256>() << "Missing material configuration: " << _splitName._settingName));

        _depVal = std::make_shared<::Assets::DependencyValidation>();

                // first, load inherited settings.
        auto* inheritList = source->ChildWithValue("Inherit");
        if (inheritList) {
            for (auto i=inheritList->child; i; i=i->next) {
                _inherit.push_back(ResString(i->value));
            }
        }

            //  Load ShaderParams & ResourceBindings & Constants

        const auto* p = source->ChildWithValue("ShaderParams");
        if (p) {
            for (auto child=p->child; child; child=child->next) {
                if (child->ChildAt(0)) {
                    _matParamBox.SetParameter((const utf8*)child->StrValue(), child->ChildAt(0)->StrValue());
                }
            }
        }

        const auto* c = source->ChildWithValue("Constants");
        if (c) {
            for (auto child=c->child; child; child=child->next) {
                if (child->ChildAt(0)) {
                    _constants.SetParameter((const utf8*)child->StrValue(), child->ChildAt(0)->StrValue());
                }
            }
        }

        const auto* resourceBindings = source->ChildWithValue("ResourceBindings");
        if (resourceBindings) {
            for (auto child=resourceBindings->child; child; child=child->next) {
                if (child->ChildAt(0)) {
                    _resourceBindings.SetParameter((const utf8*)child->StrValue(), child->ChildAt(0)->StrValue());
                }
            }
        }

            // also load "States" table. This requires a bit more parsing work
        const auto* stateSet = source->ChildWithValue("States");
        if (stateSet) {
            _stateSet = DeserializeStateSet(*stateSet);
        }

        RegisterFileDependency(_depVal, _splitName._concreteFilename.c_str());
    }

    std::unique_ptr<RawMaterial> RawMaterial::CreateNew(const ::Assets::ResChar initialiser[])
    {
        auto result = std::make_unique<RawMaterial>();
        result->_splitName = RawMatSplitName(initialiser);
        result->_depVal = std::make_shared<::Assets::DependencyValidation>();
        return std::move(result);
    }

    RawMaterial::~RawMaterial() {}

    std::unique_ptr<Data> RawMaterial::SerializeAsData() const
    {
        auto result = std::make_unique<Data>();

        if (!_inherit.empty()) {
            auto inheritBlock = std::make_unique<Data>("Inherit");
            for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
                inheritBlock->Add(new Data(i->c_str()));
            }

            result->Add(inheritBlock.release());
        }

        std::vector<std::pair<const utf8*, std::string>> matParamStringTable;
        BuildStringTable(matParamStringTable, _matParamBox);
        if (!matParamStringTable.empty()) {
            result->Add(SerializeToData("ShaderParams", matParamStringTable).release());
        }

        std::vector<std::pair<const utf8*, std::string>> constantsStringTable;
        BuildStringTable(constantsStringTable, _constants);
        if (!constantsStringTable.empty()) {
            result->Add(SerializeToData("Constants", constantsStringTable).release());
        }

        std::vector<std::pair<const utf8*, std::string>> resourceBindingsStringTable;
        BuildStringTable(resourceBindingsStringTable, _resourceBindings);
        if (!resourceBindingsStringTable.empty()) {
            result->Add(SerializeToData("ResourceBindings", resourceBindingsStringTable).release());
        }

        result->Add(SerializeStateSet("States", _stateSet).release());

        result->SetValue(GetSettingName().c_str());
        return std::move(result);
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
            TRY {
                auto& rawParams = ::Assets::GetAssetDep<RawMaterial>(i->c_str());

                ::Assets::ResChar directory[MaxPath];
                XlDirname(directory, dimof(directory), rawParams._splitName._concreteFilename.c_str());
                ::Assets::DirectorySearchRules newSearchRules = searchRules;
                newSearchRules.AddSearchDirectory(directory);

                rawParams.Resolve(result, newSearchRules, deps);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                // we still need to add a dependency, even if it's a missing file
                if (deps) AddDep(*deps, RawMatSplitName(i->c_str()));
            } CATCH_END
        }

        MergeInto(result);
        if (deps) AddDep(*deps, _splitName);
    }



}}

