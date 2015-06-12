// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Scaffold.h"
#include "ParsingUtil.h"
#include "../RenderCore/Assets/Material.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Conversion.h"
#include <string>

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static const ParameterSet::SamplerParameter* FindSamplerParameter(
        const ParameterSet& paramSet, Section sid)
    {
        for (unsigned c=0; c<paramSet.GetSamplerParameterCount(); ++c)
            if (!Equivalent(paramSet.GetSamplerParameter(c)._sid, sid))
                return &paramSet.GetSamplerParameter(c);
        return nullptr;
    }

    static const ParameterSet::SurfaceParameter* FindSurfaceParameter(
        const ParameterSet& paramSet, Section sid)
    {
        for (unsigned c=0; c<paramSet.GetSurfaceParameterCount(); ++c)
            if (!Equivalent(paramSet.GetSurfaceParameter(c)._sid, sid))
                return &paramSet.GetSurfaceParameter(c);
        return nullptr;
    }

    void AddSamplerBinding(
        RenderCore::Assets::RawMaterial& dest,
        const ParameterSet& paramSet, const utf8 bindingName[], const utf8* samplerRefStart, const utf8* samplerRefEnd)
    {
        auto* samplerParam = FindSamplerParameter(paramSet, Section(samplerRefStart, samplerRefEnd));
        if (samplerParam) {
            auto surfaceRef = samplerParam->_image;
            auto* surfaceParam = FindSurfaceParameter(paramSet, surfaceRef);
            if (surfaceParam) {
                dest._resourceBindings.SetParameter(
                    bindingName,
                    AsString(surfaceParam->_initFrom).c_str());
            } else {
                LogWarning << "Could not resolve surface reference (" << AsString(surfaceParam->_initFrom) << ")";
            }
        } else {
            LogWarning << "Could not resolve sampler reference (" << AsString(Section(samplerRefStart, samplerRefEnd)) << ")";
        }
    }

    RenderCore::Assets::RawMaterial Convert(
        const Effect& effect, const URIResolveContext& pubEles)
    {
        RenderCore::Assets::RawMaterial matSettings;

            //  Any settings from the Collada file should override what we read
            //  in the material settings file. This means that we have 
            //  clear out the settings in the Collada file if we want the .material
            //  file to show through. This pattern works best if we can use 
            //  the .material files to specify the default settings, and then use
            //  the collada data to specialise the settings of specific parts of geometry.

            // bind the texture in the "common" effects part
        auto* profile = effect.FindProfile(u("COMMON"));
        if (!profile)
            Throw(::Assets::Exceptions::FormatError("Missing common profile in effect (%s)", AsString(effect.GetName()).c_str()));

        for (const auto& t:profile->_values) {
            auto& value = t.second;
            switch (value._type) {
            case TechniqueValue::Type::Color:
                matSettings._constants.SetParameter(std::basic_string<utf8>(t.first._start, t.first._end).c_str(), value._value);
                break;

            case TechniqueValue::Type::Float:
                matSettings._constants.SetParameter(std::basic_string<utf8>(t.first._start, t.first._end).c_str(), value._value[0]);
                break;

            case TechniqueValue::Type::Texture:
                AddSamplerBinding(
                    matSettings, profile->GetParams(), 
                    std::basic_string<utf8>(t.first._start, t.first._end).c_str(),
                    value._reference._start, value._reference._end);
                break;

            default:
            case TechniqueValue::Type::Param:
                LogWarning << "Param type technique values are not supported. In effect (" << AsString(effect.GetName()) << ")";
                break;
            }
        }
            
            //  Max exporter writes some extra texture information into the <extra> part of the
            //  profile. We should get some more texture and parameter binding information from there

        auto extraValues = profile->_extra.Element(u("technique"));
        if (extraValues) {
            auto techValue = extraValues.FirstChild();
            for (;techValue; techValue=techValue.NextSibling()) {
                auto texture = techValue.Element(u("texture"));
                if (texture) {
                    auto samplerRef = texture.RawAttribute(u("texture"));
                    if (samplerRef._end > samplerRef._start)
                        AddSamplerBinding(
                            matSettings, profile->GetParams(), 
                            techValue.Name().c_str(),
                            samplerRef._start, samplerRef._end);
                } else if (techValue.Element(u("color")) || techValue.Element(u("float")) || techValue.Element(u("param"))) {
                    LogWarning << "Color, float and param type technique values not supported in <extra> part in effect (" << AsString(effect.GetName()) << ")";
                }
            }
        }
        
        return std::move(matSettings);
    }

}}
