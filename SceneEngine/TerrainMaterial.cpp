// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainMaterial.h"
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

    void TerrainMaterialScaffold::Write(OutputStream& stream) const
    {
        OutputStreamFormatter formatter(stream);
        auto cfg = formatter.BeginElement(u("Config"));
        Serialize(formatter, u("DiffuseDims"), _diffuseDims);
        Serialize(formatter, u("NormalDims"), _normalDims);
        Serialize(formatter, u("ParamDims"), _paramDims);

        auto strataList = formatter.BeginElement(u("Strata"));
        unsigned strataIndex = 0;
        for (auto s=_strata.cbegin(); s!=_strata.cend(); ++s, ++strataIndex) {
            auto strata = formatter.BeginElement((StringMeld<64, utf8>() << "Strata" << strataIndex).get());
            for (unsigned t=0; t<dimof(TextureNames); ++t)
                formatter.WriteAttribute(TextureNames[t], Conversion::Convert<std::basic_string<utf8>>(s->_texture[t]));

            Serialize(formatter, "EndHeight", s->_endHeight);
            Serialize(formatter, "Mapping", Float4(s->_mappingConstant[0], s->_mappingConstant[1], s->_mappingConstant[2], 1.f));
            formatter.EndElement(strata);
        }
        formatter.EndElement(strataList);
        formatter.EndElement(cfg);
    }

    TerrainMaterialScaffold::TerrainMaterialScaffold()
    {
        _diffuseDims = _normalDims = _paramDims = UInt2(32, 32);
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        _cachedHashValue = ~0x0ull;
    }

    TerrainMaterialScaffold::TerrainMaterialScaffold(const char definitionFile[])
    {
        size_t fileSize = 0;
        auto file = LoadFileAsMemoryBlock(definitionFile, &fileSize);
        if (!fileSize)
            ThrowException(::Exceptions::BasicLabel("Parse error while loading terrain texture list"));

        InputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(file.get(), PtrAdd(file.get(), fileSize)));
        Document<utf8> doc(formatter);

        auto cfg = doc.Element(u("Config"));

        _diffuseDims = Deserialize(cfg, u("DiffuseDims"), UInt2(512, 512));
        _normalDims = Deserialize(cfg, u("NormalDims"), UInt2(512, 512));
        _paramDims = Deserialize(cfg, u("ParamDims"), UInt2(512, 512));

        auto strata = cfg.Element(u("Strata"));
        unsigned strataCount = 0;
        for (auto c = strata.FirstChild(); c; c = c.NextSibling()) { ++strataCount; }

        unsigned strataIndex = 0;
        for (auto c = strata.FirstChild(); c; c = c.NextSibling(), ++strataIndex) {
            Strata newStrata;
            for (unsigned t=0; t<dimof(TextureNames); ++t) {
                auto tName = c.AttributeOrEmpty(TextureNames[t]);
                if (XlCompareStringI(tName.c_str(), u("null"))!=0)
                    newStrata._texture[t] = Conversion::Convert<::Assets::rstring>(tName);
            }

            newStrata._endHeight = Deserialize(c, u("EndHeight"), 0.f);
            auto mappingConst = Deserialize(c, u("Mapping"), Float4(1.f, 1.f, 1.f, 1.f));
            newStrata._mappingConstant[0] = mappingConst[0];
            newStrata._mappingConstant[1] = mappingConst[1];
            newStrata._mappingConstant[2] = mappingConst[2];

            _strata.push_back(newStrata);
        }

        _searchRules = ::Assets::DefaultDirectorySearchRules(definitionFile);
        _cachedHashValue = 0ull;

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, definitionFile);
    }

    TerrainMaterialScaffold::~TerrainMaterialScaffold() {}

    std::unique_ptr<TerrainMaterialScaffold> TerrainMaterialScaffold::CreateNew(const char definitionFile[])
    {
        auto result = std::make_unique<TerrainMaterialScaffold>();
        if (definitionFile)
            result->_searchRules = ::Assets::DefaultDirectorySearchRules(definitionFile);
        result->_validationCallback = std::make_shared<::Assets::DependencyValidation>();
        return result;
    }


}

