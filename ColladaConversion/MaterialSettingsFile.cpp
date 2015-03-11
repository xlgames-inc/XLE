// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialSettingsFile.h"
#include "../RenderCore/Metal/State.h"      // (just for Blend/BlendOp enum members)
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/Conversion.h"
#include "../Utility/Streams/FileUtils.h"


namespace RenderCore { namespace ColladaConversion
{
    static Metal::Blend::Enum DeserializeBlend(const Data* source, const char name[])
    {
        using namespace Metal::Blend;
        std::pair<Enum, const char*> names[] =
        {
            std::make_pair(Zero, "zero"),
            std::make_pair(One, "one"),
            
            std::make_pair(SrcColor, "srccolor"),
            std::make_pair(InvSrcColor, "invsrccolor"),
            std::make_pair(DestColor, "destcolor"),
            std::make_pair(InvDestColor, "invdestcolor"),

            std::make_pair(SrcAlpha, "srcalpha"),
            std::make_pair(InvSrcAlpha, "invsrcalpha"),
            std::make_pair(DestAlpha, "destalpha"),
            std::make_pair(InvDestAlpha, "invdestalpha"),
        };

        if (source) {
            auto* child = source->ChildWithValue(name);
            if (child && child->child && child->child->value) {
                const char* value = child->child->value;
                for (unsigned c=0; c<dimof(names); ++c) {
                    if (!XlCompareStringI(value, names[c].second)) {
                        return names[c].first;
                    }
                }
                return (Enum)XlAtoI32(value);
            }
        }

        return Zero;
    }

    static Metal::BlendOp::Enum DeserializeBlendOp(const Data* source, const char name[])
    {
        using namespace Metal::BlendOp;
        std::pair<Enum, const char*> names[] =
        {
            std::make_pair(NoBlending, "noblending"),
            std::make_pair(NoBlending, "none"),
            std::make_pair(NoBlending, "false"),

            std::make_pair(Add, "add"),
            std::make_pair(Subtract, "subtract"),
            std::make_pair(RevSubtract, "revSubtract"),
            std::make_pair(Min, "min"),
            std::make_pair(Max, "max")
        };

        if (source) {
            auto* child = source->ChildWithValue(name);
            if (child && child->child && child->child->value) {
                const char* value = child->child->value;
                for (unsigned c=0; c<dimof(names); ++c) {
                    if (!XlCompareStringI(value, names[c].second)) {
                        return names[c].first;
                    }
                }
                return (Enum)XlAtoI32(value);
            }
        }

        return NoBlending;
    }

    static Assets::RenderStateSet ParseStateSet(const Data& src)
    {
        typedef Assets::RenderStateSet StateSet;
        StateSet result;
        {
            auto* child = src.ChildWithValue("DoubleSided");
            if (child && child->child && child->child->value) {
                result._doubleSided = Conversion::Convert<bool>((const char*)child->child->value);
                result._flag |= StateSet::Flag::DoubleSided;
            }
        }
        {
            auto* child = src.ChildWithValue("Wireframe");
            if (child && child->child && child->child->value) {
                result._wireframe = Conversion::Convert<bool>((const char*)child->child->value);
                result._flag |= StateSet::Flag::Wireframe;
            }
        }
        {
            auto* child = src.ChildWithValue("WriteMask");
            if (child && child->child && child->child->value) {
                result._writeMask = child->child->IntValue();
                result._flag |= StateSet::Flag::WriteMask;
            }
        }
        {
            auto* child = src.ChildWithValue("DeferredBlend");
            if (child && child->child && child->child->value) {
                if (XlCompareStringI(child->child->value, "decal")) {
                    result._deferredBlend = StateSet::DeferredBlend::Decal;
                } else {
                    result._deferredBlend = StateSet::DeferredBlend::Opaque;
                }
                result._flag |= StateSet::Flag::DeferredBlend;
            }
        }
        {
            auto* child = src.ChildWithValue("DepthBias");
            if (child && child->child && child->child->value) {
                result._depthBias = child->child->IntValue();
                result._flag |= StateSet::Flag::DepthBias;
            }
        }
        {
            auto* child = src.ChildWithValue("ForwardBlend");
            if (child && child->child) {
                result._forwardBlendSrc = DeserializeBlend(child, "Src");
                result._forwardBlendDst = DeserializeBlend(child, "Dst");
                result._forwardBlendOp = DeserializeBlendOp(child, "Op");
                result._flag |= StateSet::Flag::ForwardBlend;
            }
        }
        return result;
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

    MaterialSettingsFile::MaterialDesc::MaterialDesc(
        const Data& source,
        ::Assets::DirectorySearchRules* searchRules,
        std::vector<std::shared_ptr<::Assets::DependencyValidation>>* inherited)
    {
            // first, load inherited settings.
        auto* inheritList = source.ChildWithValue("Inherit");
        if (inheritList) {
            for (auto i=inheritList->child; i; i=i->next) {
                auto* colon = XlFindCharReverse(i->value, ':');
                if (colon) {
                    ::Assets::ResChar resolvedFile[MaxPath];
                    XlCopyNString(resolvedFile, i->value, colon-i->value);
                    XlCatString(resolvedFile, dimof(resolvedFile), ".material");
                    if (searchRules) {
                        searchRules->ResolveFile(
                            resolvedFile, dimof(resolvedFile), resolvedFile);
                    }

                    MaterialSettingsFile settingsTable(resolvedFile);
                    auto settingHash = Hash64(colon+1);
                    
                    auto s = LowerBound(settingsTable._materials, settingHash);
                    if (s != settingsTable._materials.end() && s->first == settingHash) {
                        _matParamBox.MergeIn(s->second._matParamBox);
                        _stateSet = Merge(_stateSet, s->second._stateSet);
                        _resourceBindings.insert(
                            _resourceBindings.end(),
                            s->second._resourceBindings.begin(), s->second._resourceBindings.end());
                        _constants.MergeIn(s->second._constants);
                    }

                    if (inherited && std::find(inherited->begin(), inherited->end(), settingsTable.GetDependencyValidation())== inherited->end()) {
                        inherited->push_back(settingsTable.GetDependencyValidation());
                    }
                }
            }
        }

            //  Load ShaderParams & ResourceBindings & Constants

        const auto* p = source.ChildWithValue("ShaderParams");
        if (p) {
            for (auto child=p->child; child; child=child->next) {
                _matParamBox.SetParameter(
                    child->StrValue(),
                    child->ChildAt(0)?child->ChildAt(0)->IntValue():0);
            }
        }

        const auto* c = source.ChildWithValue("Constants");
        if (c) {
            for (auto child=p->child; child; child=child->next) {
                _constants.SetParameter(
                    child->StrValue(),
                    child->ChildAt(0)?child->ChildAt(0)->IntValue():0);
            }
        }

        const auto* resourceBindings = source.ChildWithValue("ResourceBindings");
        if (resourceBindings) {
            for (auto i=p->child; i; i=i->next) {
                if (i->value && i->value[0] && i->child) {
                    const char* resource = i->child->value;
                    if (resource && resource[0]) {
                        _resourceBindings.push_back(
                            Assets::MaterialParameters::ResourceBinding(
                                Hash64(i->value, &i->value[XlStringLen(i->value)]),
                                resource));
                    }
                }
            }
        }

            // also load "States" table. This requires a bit more parsing work
        const auto* stateSet = source.ChildWithValue("States");
        if (stateSet) {
            auto parsedStateSet = ParseStateSet(*stateSet);
            _stateSet = Merge(_stateSet, parsedStateSet);
        }
    }

    MaterialSettingsFile::MaterialDesc::MaterialDesc() {}
    MaterialSettingsFile::MaterialDesc::~MaterialDesc() {}

    MaterialSettingsFile::MaterialSettingsFile() {}

    MaterialSettingsFile::MaterialSettingsFile(const ResChar filename[])
    {
        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &sourceFileSize);

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);

        if (sourceFile) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(filename);
            std::vector<std::shared_ptr<::Assets::DependencyValidation>> inherited;

            Data data;
            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);

            for (auto c=data.child; c; c=c->next) {
                MaterialDesc matDesc(*c, &searchRules, &inherited);
                _materials.push_back(std::make_pair(Hash64(c->value), std::move(matDesc)));
            }

            for (auto i=inherited.begin(); i!=inherited.end(); ++i) {
                ::Assets::RegisterAssetDependency(_depVal, i->get());
            }

			_inherited = std::move(inherited);
        }

        std::sort(_materials.begin(), _materials.end(), CompareFirst<uint64, MaterialDesc>());
    }

    MaterialSettingsFile::~MaterialSettingsFile() {}
}}

