// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Scaffold.h"
#include "ConversionUtil.h"
#include "ScaffoldParsingUtil.h"
#include "../RenderCore/Assets/Material.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Conversion.h"
#include "../Utility/Streams/PathUtils.h"
#include <string>

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static const ParameterSet::SamplerParameter* FindSamplerParameter(
        const ParameterSet& paramSet, Section sid)
    {
        for (unsigned c=0; c<paramSet.GetSamplerParameterCount(); ++c)
            if (XlEqString(paramSet.GetSamplerParameter(c)._sid, sid))
                return &paramSet.GetSamplerParameter(c);
        return nullptr;
    }

    static const ParameterSet::SurfaceParameter* FindSurfaceParameter(
        const ParameterSet& paramSet, Section sid)
    {
        for (unsigned c=0; c<paramSet.GetSurfaceParameterCount(); ++c)
            if (XlEqString(paramSet.GetSurfaceParameter(c)._sid, sid))
                return &paramSet.GetSurfaceParameter(c);
        return nullptr;
    }

    void AddSamplerBinding(
        RenderCore::Assets::RawMaterial& dest,
        const ParameterSet& paramSet, const utf8 bindingName[], 
        const utf8* samplerRefStart, const utf8* samplerRefEnd,
        const URIResolveContext& resolveContext)
    {
        auto* samplerParam = FindSamplerParameter(paramSet, Section(samplerRefStart, samplerRefEnd));
        if (samplerParam) {

                // the "init_from" member of <surface> seems to reference
                // an image in the <library_images> section.
                // in Collada 1.5, there should be an instance_image into the sampler
                // object, skipping the "surface" param.
            Section imageRef = samplerParam->_instanceImage;
            if (imageRef._end == imageRef._start) {
                auto* surfaceParam = FindSurfaceParameter(paramSet, samplerParam->_source);
                if (surfaceParam) {
                    imageRef = surfaceParam->_initFrom;                    
                }
            }

            if (imageRef._end != imageRef._start) {
                GuidReference ref(imageRef);
                auto* file = resolveContext.FindFile(ref._fileHash);
                if (file) {
                    auto* image = file->FindImage(ref._id);
                    if (image) {
                        utf8 rebuiltPath[MaxPath];
                        SplitPath<utf8>(image->GetInitFrom()).Simplify().Rebuild(rebuiltPath, dimof(rebuiltPath));

                        dest._resourceBindings.SetParameter(bindingName, (const char*)rebuiltPath);
                        return;
                    }
                }
            }
    
            LogWarning << "Could not resolve surface reference (" << AsString(imageRef) << ")";
        } else {
            LogWarning << "Could not resolve sampler reference (" << AsString(Section(samplerRefStart, samplerRefEnd)) << ")";
        }
    }

    RenderCore::Assets::RawMaterial Convert(
        const Effect& effect, const URIResolveContext& pubEles,
        const ImportConfiguration& cfg)
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
            case TechniqueValue::Type::Float:
                {
                    if (cfg.GetConstantBindings().IsSuppressed(t.first._start, t.first._end)) continue;
                    auto binding = cfg.GetConstantBindings().AsNative(t.first._start, t.first._end);

                    if (value._type == TechniqueValue::Type::Color) {
                        matSettings._constants.SetParameter(
                            binding.c_str(), value._value);
                    } else {
                        matSettings._constants.SetParameter(binding.c_str(), value._value[0]);
                    }
                    break;
                }

            case TechniqueValue::Type::Texture:
                {
                    if (cfg.GetResourceBindings().IsSuppressed(t.first._start, t.first._end)) continue;
                    auto binding = cfg.GetResourceBindings().AsNative(t.first._start, t.first._end);

                    AddSamplerBinding(
                        matSettings, profile->GetParams(), 
                        binding.c_str(),
                        value._reference._start, value._reference._end,
                        pubEles);
                    break;
                }

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
                auto n = techValue.Name();
                if (cfg.GetResourceBindings().IsSuppressed(AsPointer(n.cbegin()), AsPointer(n.cend()))) continue;

                auto texture = techValue.Element(u("texture"));
                if (texture) {
                    auto samplerRef = texture.Attribute(u("texture")).Value();
                    if (!samplerRef.empty()) {
                        auto binding = cfg.GetResourceBindings().AsNative(AsPointer(n.cbegin()), AsPointer(n.cend()));
                        AddSamplerBinding(
                            matSettings, profile->GetParams(), 
                            binding.c_str(), 
                            AsPointer(samplerRef.cbegin()), AsPointer(samplerRef.cend()),
                            pubEles);
                    }
                } else if (techValue.Element(u("color")) || techValue.Element(u("float")) || techValue.Element(u("param"))) {
                    LogWarning << "Color, float and param type technique values not supported in <extra> part in effect (" << AsString(effect.GetName()) << ")";
                }
            }
        }
        
        return std::move(matSettings);
    }

}}
