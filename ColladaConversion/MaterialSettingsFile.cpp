// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialSettingsFile.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/Streams/FileUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    MaterialSettingsFile::MaterialDesc::MaterialDesc(
        const Data& source,
        ::Assets::DirectorySearchRules* searchRules,
        std::vector<const ::Assets::DependencyValidation*>* inherited)
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
                        _resourceBindings.insert(
                            _resourceBindings.end(),
                            s->second._resourceBindings.begin(), s->second._resourceBindings.end());
                    }

                    if (inherited && std::find(inherited->begin(), inherited->end(), &settingsTable.GetDependencyValidation())== inherited->end()) {
                        inherited->push_back(&settingsTable.GetDependencyValidation());
                    }
                }
            }
        }

            //  Load ShaderParams & ResourceBindings

        const auto* p = source.ChildWithValue("ShaderParams");
        if (p) {
            for (auto child=p->child; child; child=child->next) {
                _matParamBox.SetParameter(
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

            // \todo -- also load "States" table
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
            std::vector<const ::Assets::DependencyValidation*> inherited;

            Data data;
            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);

            for (auto c=data.child; c; c=c->next) {
                MaterialDesc matDesc(*c, &searchRules, &inherited);
                _materials.push_back(std::make_pair(Hash64(c->value), std::move(matDesc)));
            }

            for (auto i=inherited.begin(); i!=inherited.end(); ++i) {
                ::Assets::RegisterAssetDependency(_depVal, *i);
            }
        }
    }

    MaterialSettingsFile::~MaterialSettingsFile() {}
}}

