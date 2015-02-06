// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "AssetUtils.h"
#include "TransformationCommands.h"
#include "../RenderUtils.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/OutputStream.h"
#include "../../Utility/Streams/Stream.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Math/Vector.h"
#include "../../Math/Transformations.h"

#include "../Metal/State.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Format.h"
#include <stdarg.h>

#include "../../SceneEngine/ResourceBox.h"

    // for debugging rendering
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Techniques.h"

namespace RenderCore { namespace Assets
{
    void Warning(const char format[], ...)
    {
        va_list args;
        va_start(args, format);
        ConsoleRig::GetWarningStream().WriteString((const utf8*)"{Color:ff7f7f}");
        PrintFormatV(&ConsoleRig::GetWarningStream(), format, args);
        va_end(args);
    }

    Metal::ConstantBufferLayoutElement GlobalTransform_Elements[] = {
        { "WorldToClip", Metal::NativeFormat::Matrix4x4, offsetof(GlobalTransformConstants, _worldToClip), 0 },
        { "FrustumCorners", Metal::NativeFormat::R32G32B32A32_FLOAT, offsetof(GlobalTransformConstants, _frustumCorners), 4 },
        { "WorldSpaceView", Metal::NativeFormat::R32G32B32_FLOAT, offsetof(GlobalTransformConstants, _worldSpaceView), 0 }
    };

    size_t GlobalTransform_ElementsCount = dimof(GlobalTransform_Elements);

    struct MaterialProperties
    {
    public:
        float		RefractiveIndex;
		float		Shininess;
		float		Rho;
		float		Ambient;
		Float4		SpecularColour;
    };

    Metal::ConstantBufferLayoutElement MaterialProperties_Elements[] = 
    {
        { "RefractiveIndex",        Metal::NativeFormat::R32_FLOAT,             offsetof(MaterialProperties, RefractiveIndex), 0 },
        { "Shininess",              Metal::NativeFormat::R32_FLOAT,             offsetof(MaterialProperties, Shininess), 0 },
        { "Rho",                    Metal::NativeFormat::R32_FLOAT,             offsetof(MaterialProperties, Rho), 0 },
        { "Ambient",                Metal::NativeFormat::R32_FLOAT,             offsetof(MaterialProperties, Ambient), 0 },
        { "SpecularColour",         Metal::NativeFormat::R32G32B32A32_FLOAT,    offsetof(MaterialProperties, SpecularColour), 0 }
    };

    size_t MaterialProperties_ElementsCount = dimof(MaterialProperties_Elements);


    Metal::ConstantBufferPacket DefaultMaterialProperties()
    {
        MaterialProperties materialProperties;
        materialProperties.RefractiveIndex          = Tweakable("RefractiveIndex", 1.4f);
        materialProperties.Shininess                = Tweakable("Shininess", 32.f);
        materialProperties.Rho                      = Tweakable("Rho", 4.f);
        materialProperties.Ambient                  = Tweakable("Ambient", .15f);
        materialProperties.SpecularColour           = Float4(Tweakable("SpecularColourR", 40.f), Tweakable("SpecularColourG", 40.f), Tweakable("SpecularColourB", 40.f), 1.f);
        return MakeSharedPkt(materialProperties);
    }

        ////////////////////////////////////////////////////////////

    Metal::ConstantBufferLayoutElement Assets::LocalTransform_Elements[] = {
        { "LocalToWorld",                   Metal::NativeFormat::Matrix3x4,        offsetof(LocalTransformConstants, _localToWorld), 0                  },
        { "LocalSpaceView",                 Metal::NativeFormat::R32G32B32_FLOAT,  offsetof(LocalTransformConstants, _localSpaceView), 0                },
        { "LocalNegativeLightDirection",    Metal::NativeFormat::R32G32B32_FLOAT,  offsetof(LocalTransformConstants, _localNegativeLightDirection), 0   }
    };

    size_t Assets::LocalTransform_ElementsCount = dimof(Assets::LocalTransform_Elements);

        ///////////////////////////////////////////////////////

    static void MakeIndentBuffer(char buffer[], unsigned bufferSize, signed identLevel)
    {
        std::fill(buffer, &buffer[std::min(std::max(0,identLevel), signed(bufferSize-1))], ' ');
        buffer[std::min(std::max(0,identLevel), signed(bufferSize-1))] = '\0';
    }

    void TraceTransformationMachine(
            Utility::OutputStream&      outputStream,
            const uint32*               commandStreamBegin,
            const uint32*               commandStreamEnd)
    {
        PrintFormat(&outputStream, "Transformation machine size: (%i) bytes\n", 
            commandStreamEnd - commandStreamBegin);

        char indentBuffer[32];
        signed indentLevel = 2;
        MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);

        for (auto i=commandStreamBegin; i!=commandStreamEnd;) {
            auto commandIndex = *i++;
            switch (commandIndex) {
            case TransformStackCommand::PushLocalToWorld:
                PrintFormat(&outputStream, "%sPushLocalToWorld\n", indentBuffer);
                ++indentLevel;
                MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                break;

            case TransformStackCommand::PopLocalToWorld:
                {
                    auto popCount = *i++;
                    PrintFormat(&outputStream, "%sPopLocalToWorld (%i)\n", indentBuffer, popCount);
                    indentLevel -= popCount;
                    MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                }
                break;

            case TransformStackCommand::TransformFloat4x4_Static:
                PrintFormat(&outputStream, "%sTransformFloat4x4_Static\n");
                i += 16;
                break;

            case TransformStackCommand::Translate_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    PrintFormat(&outputStream, "%sTranslate_Static (%f, %f, %f)\n", indentBuffer, trans[0], trans[1], trans[2]);
                    i += 3;
                }
                break;

            case TransformStackCommand::RotateX_Static:
                PrintFormat(&outputStream, "%sRotateX_Static (%f)\n", indentBuffer, *reinterpret_cast<const float*>(AsPointer(i)));
                i++;
                break;

            case TransformStackCommand::RotateY_Static:
                PrintFormat(&outputStream, "%sRotateY_Static (%f)\n", indentBuffer, *reinterpret_cast<const float*>(AsPointer(i)));
                i++;
                break;

            case TransformStackCommand::RotateZ_Static:
                PrintFormat(&outputStream, "%sRotateZ_Static (%f)\n", indentBuffer, *reinterpret_cast<const float*>(AsPointer(i)));
                i++;
                break;

            case TransformStackCommand::Rotate_Static:
                {
                    auto a = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    float r = *reinterpret_cast<const float*>(AsPointer(i+3));
                    PrintFormat(&outputStream, "%sRotate_Static (%f, %f, %f)(%f)\n", indentBuffer, a[0], a[1], a[2], r);
                    i += 4;
                }
                break;

            case TransformStackCommand::UniformScale_Static:
                PrintFormat(&outputStream, "%sUniformScale_Static (%f)\n", indentBuffer, *reinterpret_cast<const float*>(AsPointer(i)));
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    PrintFormat(&outputStream, "%sArbitraryScale_Static (%f, %f, %f)\n", indentBuffer, trans[0], trans[1], trans[2]);
                }
                i+=3;
                break;

            case TransformStackCommand::TransformFloat4x4_Parameter:
                PrintFormat(&outputStream, "%sTransformFloat4x4_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::Translate_Parameter:
                PrintFormat(&outputStream, "%sTranslate_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::RotateX_Parameter:
                PrintFormat(&outputStream, "%sRotateX_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::RotateY_Parameter:
                PrintFormat(&outputStream, "%sRotateY_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::RotateZ_Parameter:
                PrintFormat(&outputStream, "%sRotateZ_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::Rotate_Parameter:
                PrintFormat(&outputStream, "%sRotate_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::UniformScale_Parameter:
                PrintFormat(&outputStream, "%sUniformScale_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Parameter:
                PrintFormat(&outputStream, "%sArbitraryScale_Parameter (%i)\n", indentBuffer, *i);
                i++;
                break;

            case TransformStackCommand::WriteOutputMatrix:
                PrintFormat(&outputStream, "%sWriteOutputMatrix (%i)\n", indentBuffer, *i);
                i++;
                break;
            }

            assert(i <= commandStreamEnd);  // make sure we haven't jumped past the end marker
        }
    }


        ////////////////////////////////////////////////////////////

    void MaterialParameters::ResourceBinding::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _bindHash);
        Serialization::Serialize(serializer, _resourceName);
    }
    
    void MaterialParameters::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _bindings);
    }

    MaterialParameters::MaterialParameters() {}

    MaterialParameters::MaterialParameters(MaterialParameters&& moveFrom)
    : _bindings(std::move(moveFrom._bindings))
    {}

    MaterialParameters& MaterialParameters::operator=(MaterialParameters&& moveFrom)
    {
        _bindings = std::move(moveFrom._bindings);
        return *this;
    }

        ////////////////////////////////////////////////////////////

    TransformationParameterSet::TransformationParameterSet()
    {
    }

    TransformationParameterSet::TransformationParameterSet(TransformationParameterSet&& moveFrom)
    :       _float4x4Parameters(    std::move(moveFrom._float4x4Parameters))
    ,       _float4Parameters(      std::move(moveFrom._float4Parameters))
    ,       _float3Parameters(      std::move(moveFrom._float3Parameters))
    ,       _float1Parameters(      std::move(moveFrom._float1Parameters))
    {

    }

    TransformationParameterSet& TransformationParameterSet::operator=(TransformationParameterSet&& moveFrom)
    {
        _float4x4Parameters = std::move(moveFrom._float4x4Parameters);
        _float4Parameters   = std::move(moveFrom._float4Parameters);
        _float3Parameters   = std::move(moveFrom._float3Parameters);
        _float1Parameters   = std::move(moveFrom._float1Parameters);
        return *this;
    }

    TransformationParameterSet::TransformationParameterSet(const TransformationParameterSet& copyFrom)
    :       _float4x4Parameters(copyFrom._float4x4Parameters)
    ,       _float4Parameters(copyFrom._float4Parameters)
    ,       _float3Parameters(copyFrom._float3Parameters)
    ,       _float1Parameters(copyFrom._float1Parameters)
    {
    }

    TransformationParameterSet&  TransformationParameterSet::operator=(const TransformationParameterSet& copyFrom)
    {
        _float4x4Parameters = copyFrom._float4x4Parameters;
        _float4Parameters = copyFrom._float4Parameters;
        _float3Parameters = copyFrom._float3Parameters;
        _float1Parameters = copyFrom._float1Parameters;
        return *this;
    }

    void    TransformationParameterSet::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        Serialization::Serialize(outputSerializer, _float4x4Parameters);
        Serialization::Serialize(outputSerializer, _float4Parameters);
        Serialization::Serialize(outputSerializer, _float3Parameters);
        Serialization::Serialize(outputSerializer, _float1Parameters);
    }

        ////////////////////////////////////////////////////////////

    class CachedTextureFormats
    {
    public:
        class Desc {};

        CachedTextureFormats(const Desc&);
        ~CachedTextureFormats();

        typedef std::pair<uint64, uint32> Entry;
        std::unique_ptr<MemoryMappedFile> _cache;

        class Header
        {
        public:
            unsigned _count;
        };

        static const unsigned MaxCachedTextures = 10*1024;
    };

    CachedTextureFormats::CachedTextureFormats(const Desc&)
    {
        unsigned entrySize = sizeof(Entry);
            
            //  use a memory mapped file for this. This way, we never have to 
            //  worry about flushing out to disk... The OS will take care of 
            //  comitting the results to disk on exit
        auto cache = std::make_unique<MemoryMappedFile>(
            "int/TextureFormatCache.dat", entrySize * MaxCachedTextures + sizeof(Header),
            MemoryMappedFile::Access::Read|MemoryMappedFile::Access::Write|MemoryMappedFile::Access::OpenAlways);
        _cache = std::move(cache);
    }

    CachedTextureFormats::~CachedTextureFormats() {}

    bool IsDXTNormalMap(const std::string& textureName)
    {
        if (textureName.empty()) return false;

        auto& cache = SceneEngine::FindCachedBox<CachedTextureFormats>(
            CachedTextureFormats::Desc());

        typedef CachedTextureFormats::Header Hdr;
        typedef CachedTextureFormats::Entry Entry;
        auto* data = cache._cache->GetData();
        auto& hdr = *(Hdr*)data;
        auto* start = (Entry*)PtrAdd(data, sizeof(Hdr));
        auto* end = (Entry*)PtrAdd(data, sizeof(Hdr) + sizeof(Entry) * hdr._count);

        auto hashName = Hash64(textureName);
        auto* i = std::lower_bound(start, end, hashName, CompareFirst<uint64, uint32>());
        if (i == end || i->first != hashName) {
            if ((hdr._count+1) > CachedTextureFormats::MaxCachedTextures) {
                assert(0);  // cache has gotten too big
                return false;
            }

            std::move_backward(i, end, end+1);
            i->first = hashName;
            TRY {
                i->second = (uint32)RenderCore::Metal::LoadTextureFormat(textureName.c_str());
            } CATCH (const ::Assets::Exceptions::InvalidResource&) {
                i->second = RenderCore::Metal::NativeFormat::Unknown;
            } CATCH_END
            ++hdr._count;
            return i->second >= RenderCore::Metal::NativeFormat::BC1_TYPELESS
                && i->second <= RenderCore::Metal::NativeFormat::BC1_UNORM_SRGB;
        }

        return      i->second >= RenderCore::Metal::NativeFormat::BC1_TYPELESS
                &&  i->second <= RenderCore::Metal::NativeFormat::BC1_UNORM_SRGB;
    }
}}

