// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SharedStateSet.h"
#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/Buffer.h"
#include "../../SceneEngine/Techniques.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/ParameterBox.h"
#include <vector>

namespace RenderCore { namespace Assets
{
    class SharedStateSet::Pimpl
    {
    public:
        std::vector<std::string>                        _shaderNames;
        std::vector<SceneEngine::TechniqueInterface>    _techniqueInterfaces;
        std::vector<ParameterBox>                       _parameterBoxes;
        std::vector<uint64>                             _techniqueInterfaceHashes;
    };

    static uint64 Hash(const Metal::InputElementDesc& desc)
    {
            //  hash the semantic name and the scalar parameters
            //  Note that sometimes there might be a chance of equivalent
            //  elements producing different hash values
            //      (for example, _alignedByteOffset can be ~unsigned(0x0) to
            //      choose the default offset -- so if another desc explicitly
            //      sets the offset, the values will be different)
        auto t = Hash64(desc._semanticName);
        t ^= Hash64(&desc._semanticIndex, PtrAdd(&desc._instanceDataStepRate, sizeof(unsigned)));
        return t;
    }

    unsigned SharedStateSet::InsertTechniqueInterface(
        const RenderCore::Metal::InputElementDesc vertexElements[], unsigned count,
        const uint64 textureBindPoints[], unsigned textureBindPointsCount)
    {
        uint64 interfHash = 0;
        for (unsigned e=0; e<count; ++e) {
            interfHash ^= Hash(vertexElements[e]);
        }
        for (unsigned e=0; e<textureBindPointsCount; ++e) {
            interfHash ^= textureBindPoints[e];
        }

        unsigned techniqueInterfaceIndex = 0;
        auto& hashes = _pimpl->_techniqueInterfaceHashes;
        auto& interfaces = _pimpl->_techniqueInterfaces;

        auto existingInterface = std::find(hashes.cbegin(), hashes.cend(), interfHash);
        if (existingInterface == hashes.cend()) {
                //  No existing interface. We have to build a new one.
            SceneEngine::TechniqueInterface techniqueInterface(
                Metal::InputLayout(vertexElements, count));

            static const auto HashLocalTransform = Hash64("LocalTransform");
            static const auto HashBasicMaterial = Hash64("BasicMaterialConstants");
            techniqueInterface.BindConstantBuffer(HashLocalTransform, 0, 1);
            techniqueInterface.BindConstantBuffer(HashBasicMaterial, 1, 1);
            SceneEngine::TechniqueContext::BindGlobalUniforms(techniqueInterface);
            for (unsigned c=0; c<textureBindPointsCount; ++c) {
                techniqueInterface.BindShaderResource(textureBindPoints[c], c, 1);
            }
                
            interfaces.push_back(std::move(techniqueInterface));
            hashes.push_back(interfHash);
            techniqueInterfaceIndex = interfaces.size()-1;
        } else {
            techniqueInterfaceIndex = existingInterface - hashes.cbegin();
        }

        return techniqueInterfaceIndex;
    }

    unsigned SharedStateSet::InsertShaderName(const std::string& shaderName)
    {
        auto& shaderNames = _pimpl->_shaderNames;
        auto n = std::find(shaderNames.cbegin(), shaderNames.cend(), shaderName);
        if (n == shaderNames.cend()) {
            shaderNames.push_back(shaderName);
            return shaderNames.size()-1;
        } else {
            return std::distance(shaderNames.cbegin(), n);
        }
    }

    unsigned SharedStateSet::InsertParameterBox(const ParameterBox& box)
    {
        auto& paramBoxes = _pimpl->_parameterBoxes;
        auto boxHash = box.GetHash();
        auto namesHash = box.GetParameterNamesHash();
        auto p = std::find_if(
            paramBoxes.cbegin(), paramBoxes.cend(), 
            [=](const ParameterBox& box) 
            { 
                return box.GetHash() == boxHash && box.GetParameterNamesHash() == namesHash; 
            });
        if (p == paramBoxes.cend()) {
            paramBoxes.push_back(box);
            return paramBoxes.size()-1;
        } else {
            return std::distance(paramBoxes.cbegin(), p);
        }
    }

    RenderCore::Metal::BoundUniforms* SharedStateSet::BeginVariation(
            Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext,
            unsigned techniqueIndex, unsigned shaderName, unsigned techniqueInterface, 
            unsigned geoParamBox, unsigned materialParamBox) const
    {
        if (    shaderName == _currentShaderName && techniqueInterface == _currentTechniqueInterface 
            &&  geoParamBox == _currentGeoParamBox && materialParamBox == _currentMaterialParamBox) {
            return _currentBoundUniforms;
        }

            // we need to check both the "xleres" and "xleres_cry" folders for material files
        char buffer[MaxPath];
        XlCopyString(buffer, "game/xleres/");
        XlCatString(buffer, dimof(buffer), _pimpl->_shaderNames[shaderName].c_str());
        XlCatString(buffer, dimof(buffer), ".txt");

        if (!DoesFileExist(buffer)) {
            XlCopyString(buffer, "game/xleres_cry/");
            XlCatString(buffer, dimof(buffer), _pimpl->_shaderNames[shaderName].c_str());
            XlCatString(buffer, dimof(buffer), ".txt");
        }

        auto& shaderType = ::Assets::GetAssetDep<SceneEngine::ShaderType>(buffer);
        const ParameterBox* state[] = {
            &_pimpl->_parameterBoxes[geoParamBox],
            &parserContext.GetTechniqueContext()._globalEnvironmentState,
            &parserContext.GetTechniqueContext()._runtimeState,
            &_pimpl->_parameterBoxes[materialParamBox]
        };

        auto& techniqueInterfaceObj = _pimpl->_techniqueInterfaces[techniqueInterface];

            // (FindVariation can throw pending/invalid resource)
        auto variation = shaderType.FindVariation(techniqueIndex, state, techniqueInterfaceObj);
        if (variation._shaderProgram && variation._boundLayout) {
            context->Bind(*variation._shaderProgram);
            context->Bind(*variation._boundLayout);
        }

        _currentShaderName = shaderName;
        _currentTechniqueInterface = techniqueInterface;
        _currentMaterialParamBox = materialParamBox;
        _currentGeoParamBox = geoParamBox;
        _currentBoundUniforms = variation._boundUniforms;
        return _currentBoundUniforms;
    }

    void SharedStateSet::Reset()
    {
        _currentShaderName = ~unsigned(0x0);
        _currentTechniqueInterface = ~unsigned(0x0);
        _currentMaterialParamBox = ~unsigned(0x0);
        _currentGeoParamBox = ~unsigned(0x0);
        _currentBoundUniforms = nullptr;
    }


    SharedStateSet::SharedStateSet()
    {
        auto pimpl = std::make_unique<Pimpl>();

        _currentShaderName = ~unsigned(0x0);
        _currentTechniqueInterface = ~unsigned(0x0);
        _currentMaterialParamBox = ~unsigned(0x0);
        _currentGeoParamBox = ~unsigned(0x0);
        _currentBoundUniforms = nullptr;

        _pimpl = std::move(pimpl);
    }

    SharedStateSet::~SharedStateSet()
    {}

}}

