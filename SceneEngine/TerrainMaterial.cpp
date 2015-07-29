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

    void TerrainMaterialConfig::Write(OutputStreamFormatter& formatter) const
    {
        Serialize(formatter, u("DiffuseDims"), _diffuseDims);
        Serialize(formatter, u("NormalDims"), _normalDims);
        Serialize(formatter, u("ParamDims"), _paramDims);

        for (auto mat=_strataMaterials.cbegin(); mat!=_strataMaterials.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("StrataMaterial"));
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
            formatter.EndElement(matEle);
        }

        for (auto mat=_gradFlagMaterials.cbegin(); mat!=_gradFlagMaterials.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("GradFlagMaterial"));
            Serialize(formatter, "MaterialId", mat->_id);
            
            Serialize(formatter, "Texture0", mat->_texture[0]);
            Serialize(formatter, "Texture1", mat->_texture[1]);
            Serialize(formatter, "Texture2", mat->_texture[2]);
            Serialize(formatter, "Texture3", mat->_texture[3]);
            Serialize(formatter, "Texture4", mat->_texture[4]);
            using namespace ImpliedTyping;
            Serialize(formatter, "Mapping", 
                AsString(
                    mat->_mappingConstant, sizeof(mat->_mappingConstant),
                    TypeDesc(TypeCat::Float, dimof(mat->_mappingConstant)), true));
            formatter.EndElement(matEle);
        }

        for (auto mat=_procTextures.cbegin(); mat!=_procTextures.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("ProcTextureSetting"));
            Serialize(formatter, "Name", mat->_name);
            Serialize(formatter, "Texture0", mat->_texture[0]);
            Serialize(formatter, "Texture1", mat->_texture[1]);
            Serialize(formatter, "HGrid", mat->_hgrid);
            Serialize(formatter, "Gain", mat->_gain);
            formatter.EndElement(matEle);
        }
    }

    TerrainMaterialConfig::TerrainMaterialConfig()
    {
        _diffuseDims = _normalDims = _paramDims = UInt2(32, 32);
    }

    template<typename InputType>
        ::Assets::rstring AsRString(InputType input) { return Conversion::Convert<::Assets::rstring>(input); }

    TerrainMaterialConfig::TerrainMaterialConfig(
        InputStreamFormatter<utf8>& formatter,
        const ::Assets::DirectorySearchRules& searchRules)
    : TerrainMaterialConfig()
    {
        Document<InputStreamFormatter<utf8>> doc(formatter);

        _diffuseDims = doc(u("DiffuseDims"), _diffuseDims);
        _normalDims = doc(u("NormalDims"), _normalDims);
        _paramDims = doc(u("ParamDims"), _paramDims);

        for (auto matCfg=doc.FirstChild(); matCfg; matCfg=matCfg.NextSibling()) {

            if (XlEqString(matCfg.Name(), u("StrataMaterial"))) {
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
            } else if (XlEqString(matCfg.Name(), u("GradFlagMaterial"))) {

                GradFlagMaterial mat;
                mat._id = Deserialize(matCfg, u("MaterialId"), 0);
            
                mat._texture[0] = AsRString(matCfg.Attribute(u("Texture0")).Value());
                mat._texture[1] = AsRString(matCfg.Attribute(u("Texture1")).Value());
                mat._texture[2] = AsRString(matCfg.Attribute(u("Texture2")).Value());
                mat._texture[3] = AsRString(matCfg.Attribute(u("Texture3")).Value());
                mat._texture[4] = AsRString(matCfg.Attribute(u("Texture4")).Value());

                char buffer[512];
                auto mappingAttr = matCfg.Attribute(u("Mapping")).Value();
                auto parsedType = ImpliedTyping::Parse(
                    (const char*)AsPointer(mappingAttr.cbegin()), (const char*)AsPointer(mappingAttr.cend()),
                    buffer, sizeof(buffer));
                ImpliedTyping::Cast(
                    mat._mappingConstant, sizeof(mat._mappingConstant), 
                    ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, dimof(mat._mappingConstant)),
                    buffer, parsedType);
                
                _gradFlagMaterials.push_back(mat);

            } else if (XlEqString(matCfg.Name(), u("GradFlagMaterial"))) {

                ProcTextureSetting mat;
                mat._name = AsRString(matCfg.Attribute(u("Name")).Value());
                mat._texture[0] = AsRString(matCfg.Attribute(u("Texture0")).Value());
                mat._texture[1] = AsRString(matCfg.Attribute(u("Texture1")).Value());
                mat._hgrid = Deserialize(matCfg, u("HGrid"), mat._hgrid);
                mat._gain = Deserialize(matCfg, u("Gain"), mat._gain);
                _procTextures.push_back(mat);

            }
        }

        _searchRules = searchRules;
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

