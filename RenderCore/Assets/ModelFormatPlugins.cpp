// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelFormatPlugins.h"
#include "ModelRunTime.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/WinApI/WinAPIWrapper.h"
#include "../../Core/SelectConfiguration.h"

namespace RenderCore { namespace Assets 
{

    auto ModelFormat_Plugins::CreateModel(const ::Assets::ResChar initializer[]) -> std::shared_ptr<ModelScaffold>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateModel(initializer);
            if (res) return res;
        }
        return nullptr;
    }

    auto ModelFormat_Plugins::CreateMaterial(const ::Assets::ResChar initializer[]) -> std::shared_ptr<MaterialScaffold>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateMaterial(initializer);
            if (res) return res;
        }
        return nullptr;
    }

    auto ModelFormat_Plugins::CreateRenderer(
        ModelScaffold& scaffold, MaterialScaffold& material,
        SharedStateSet& sharedStateSet, unsigned levelOfDetail) -> std::shared_ptr<ModelRenderer>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateRenderer(scaffold, material, sharedStateSet, levelOfDetail);
            if (res) return res;
        }
        // return std::make_shared<ModelRenderer>(scaffold, material, sharedStateSet, levelOfDetail);
        return nullptr;
    }

    std::string ModelFormat_Plugins::DefaultMaterialName(const ModelScaffold& model)
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->DefaultMaterialName(model);
            if (!res.empty()) return res;
        }
        return std::string();
    }

    ModelFormat_Plugins::ModelFormat_Plugins()
    {
        std::vector<Plugin> plugins;

            // note --  this behaviour is Win32 specific currently!
            //          
        #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS

            #if defined(_DEBUG)
                #if TARGET_64BIT
                    #define CONFIG_DIR "Debug64"
                #else
                    #define CONFIG_DIR "Debug32"
                #endif
            #else
                #if TARGET_64BIT
                    #define CONFIG_DIR "Release64"
                #else
                    #define CONFIG_DIR "Release32"
                #endif
            #endif
            const char* pluginSearch = "../PluginNonDist/" CONFIG_DIR "/*.dll";
            auto files = FindFiles(pluginSearch, FindFilesFilter::File);
            for (auto i=files.cbegin(); i!=files.cend(); ++i) {
                auto lib = (*Windows::Fn_LoadLibrary)(i->c_str());
                if (lib && lib != INVALID_HANDLE_VALUE) {
                    const char CreateInterfaceName[] = "?CreateModelFormatInterface@@YA?AV?$unique_ptr@VIModelFormat@Assets@RenderCore@@U?$default_delete@VIModelFormat@Assets@RenderCore@@@std@@@std@@XZ";
                    const char GetVersionName[] = "?GetVersionInformation@@YA?AU?$pair@PBDPBD@std@@XZ";

                    typedef std::pair<const char*, const char*> (GetVersionFn)();
                    typedef std::unique_ptr<RenderCore::Assets::IModelFormat> (CreateInterfaceFn)();
                    GetVersionFn* fn0 = (GetVersionFn*)(*Windows::Fn_GetProcAddress)(lib, GetVersionName);
                    CreateInterfaceFn* fn1 = (CreateInterfaceFn*)(*Windows::Fn_GetProcAddress)(lib, CreateInterfaceName);

                    if (fn0 && fn1) {
                        Plugin newPlugin;
                        newPlugin._pluginInterface = (*fn1)();
                        newPlugin._versionInfo = (*fn0)();
                        newPlugin._name = *i;
                        if (newPlugin._pluginInterface) {
                            plugins.push_back(std::move(newPlugin));
                        }
                    }
                }
            }
        #endif

        _plugins = std::move(plugins);
    }

    ModelFormat_Plugins::~ModelFormat_Plugins()
    {}

}}

