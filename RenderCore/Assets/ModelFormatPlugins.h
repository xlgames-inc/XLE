// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IModelFormat.h"

namespace RenderCore { namespace Assets {

    class ModelFormat_Plugins : public IModelFormat
    {
    public:
        virtual std::shared_ptr<ModelScaffold>      CreateModel(const ::Assets::ResChar initializer[]);
        virtual std::shared_ptr<MaterialScaffold>   CreateMaterial(const ::Assets::ResChar initializer[]);
        virtual std::shared_ptr<ModelRenderer>      CreateRenderer(
            ModelScaffold& scaffold, MaterialScaffold& material,
            SharedStateSet& sharedStateSet, unsigned levelOfDetail);

        virtual std::string DefaultMaterialName(const ModelScaffold&);

        ModelFormat_Plugins();
        ~ModelFormat_Plugins();

    protected:
        class Plugin
        {
        public:
            std::pair<const char*, const char*> _versionInfo;
            std::string _name;
            std::unique_ptr<IModelFormat> _pluginInterface;

            Plugin() {}
            Plugin(Plugin&& moveFrom)
                : _versionInfo(moveFrom._versionInfo), _name(moveFrom._name), _pluginInterface(std::move(moveFrom._pluginInterface)) {}
            Plugin& operator=(Plugin&& moveFrom)
            {
                _versionInfo = moveFrom._versionInfo; _name = moveFrom._name; _pluginInterface = std::move(moveFrom._pluginInterface);
                return *this;
            }
        private:
            Plugin& operator=(const Plugin&);
            Plugin(const Plugin&);
        };

        std::vector<Plugin> _plugins;

    private:
        ModelFormat_Plugins& operator=(const ModelFormat_Plugins&);
        ModelFormat_Plugins(const ModelFormat_Plugins&);
    };

}}

