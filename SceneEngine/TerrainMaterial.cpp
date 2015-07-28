// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainMaterial.h"
#include "../Assets/AssetServices.h"
#include "../Assets/InvalidAssetManager.h"

#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/FileUtils.h"

#include "../Utility/StringFormat.h"
#include "../Utility/Conversion.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/ParameterBox.h"

namespace SceneEngine
{

    static const utf8* TextureNames[] = { u("Texture0"), u("Texture1"), u("Slopes") };

    void TerrainMaterialConfig::Write(OutputStream& stream) const
    {
        OutputStreamFormatter formatter(stream);
        auto cfg = formatter.BeginElement(u("Config"));
        Serialize(formatter, u("DiffuseDims"), _diffuseDims);
        Serialize(formatter, u("NormalDims"), _normalDims);
        Serialize(formatter, u("ParamDims"), _paramDims);

        for (auto mat = _strataMaterials.cbegin(); mat != _strataMaterials.cend(); ++mat) {
            formatter.BeginElement(u("StrataMaterial"));
            Serialize(formatter, "MaterialId", mat->_id);

            auto strataList = formatter.BeginElement(u("Strata"));
            unsigned strataIndex = 0;
            for (auto s=mat->_strata.cbegin(); s!=mat->_strata.cend(); ++s, ++strataIndex) {
                auto strata = formatter.BeginElement((StringMeld<64, utf8>() << "Strata" << strataIndex).get());
                for (unsigned t=0; t<dimof(TextureNames); ++t)
                    formatter.WriteAttribute(TextureNames[t], Conversion::Convert<std::basic_string<utf8>>(s->_texture[t]));

                Serialize(formatter, "EndHeight", s->_endHeight);
                Serialize(formatter, "Mapping", Float4(s->_mappingConstant[0], s->_mappingConstant[1], s->_mappingConstant[2], 1.f));
                formatter.EndElement(strata);
            }
            formatter.EndElement(strataList);
        }

        formatter.EndElement(cfg);
    }

    TerrainMaterialConfig::TerrainMaterialConfig()
    {
        _diffuseDims = _normalDims = _paramDims = UInt2(32, 32);
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
    }

    TerrainMaterialConfig::TerrainMaterialConfig(const char definitionFile[])
    {
        size_t fileSize = 0;
        auto file = LoadFileAsMemoryBlock(definitionFile, &fileSize);
        if (!fileSize)
            Throw(::Exceptions::BasicLabel("Parse error while loading terrain texture list"));

        TRY
        {
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(file.get(), PtrAdd(file.get(), fileSize)));
            Document<InputStreamFormatter<utf8>> doc(formatter);

            auto cfg = doc.Element(u("Config"));

            _diffuseDims = Deserialize(cfg, u("DiffuseDims"), UInt2(512, 512));
            _normalDims = Deserialize(cfg, u("NormalDims"), UInt2(512, 512));
            _paramDims = Deserialize(cfg, u("ParamDims"), UInt2(512, 512));

            for (auto matCfg=cfg.FirstChild(); matCfg; matCfg=matCfg.NextSibling()) {
                StrataMaterial mat;

                mat._id = Deserialize(matCfg, u("MaterialId"), 0u);

                auto strata = matCfg.Element(u("Strata"));
                unsigned strataCount = 0;
                for (auto c = strata.FirstChild(); c; c = c.NextSibling()) { ++strataCount; }

                unsigned strataIndex = 0;
                for (auto c = strata.FirstChild(); c; c = c.NextSibling(), ++strataIndex) {
                    StrataMaterial::Strata newStrata;
                    for (unsigned t=0; t<dimof(TextureNames); ++t) {
                        auto tName = c.Attribute(TextureNames[t]).Value();
                        if (XlCompareStringI(tName.c_str(), u("null"))!=0)
                            newStrata._texture[t] = Conversion::Convert<::Assets::rstring>(tName);
                    }

                    newStrata._endHeight = Deserialize(c, u("EndHeight"), 0.f);
                    auto mappingConst = Deserialize(c, u("Mapping"), Float4(1.f, 1.f, 1.f, 1.f));
                    newStrata._mappingConstant[0] = mappingConst[0];
                    newStrata._mappingConstant[1] = mappingConst[1];
                    newStrata._mappingConstant[2] = mappingConst[2];

                    mat._strata.push_back(newStrata);
                }

                _strataMaterials.push_back(std::move(mat));
            }

            ::Assets::Services::GetInvalidAssetMan().MarkValid(definitionFile);
        } CATCH (const std::exception& e) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(definitionFile, e.what());
            throw;
        } CATCH(...) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(definitionFile, "Unknown error");
            throw;
        } CATCH_END

        _searchRules = ::Assets::DefaultDirectorySearchRules(definitionFile);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, definitionFile);
    }

    TerrainMaterialConfig::~TerrainMaterialConfig() {}

    // std::unique_ptr<TerrainMaterialConfig> TerrainMaterialConfig::CreateNew(const char definitionFile[])
    // {
    //     auto result = std::make_unique<TerrainMaterialConfig>();
    //     if (definitionFile)
    //         result->_searchRules = ::Assets::DefaultDirectorySearchRules(definitionFile);
    //     result->_validationCallback = std::make_shared<::Assets::DependencyValidation>();
    //     return result;
    // }


}

