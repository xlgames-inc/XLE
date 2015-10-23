// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "AssetUtils.h"
#include "TransformationCommands.h"
#include "DeferredShaderResource.h"
#include "ModelScaffoldInternal.h"
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

#include "../Techniques/ResourceBox.h"
#include "../Techniques/Techniques.h"
#include "../Techniques/TechniqueUtils.h"

namespace RenderCore { namespace Assets
{
    Metal::ConstantBufferLayoutElement GlobalTransform_Elements[] = {
        { "WorldToClip", Metal::NativeFormat::Matrix4x4, offsetof(Techniques::GlobalTransformConstants, _worldToClip), 0 },
        { "FrustumCorners", Metal::NativeFormat::R32G32B32A32_FLOAT, offsetof(Techniques::GlobalTransformConstants, _frustumCorners), 4 },
        { "WorldSpaceView", Metal::NativeFormat::R32G32B32_FLOAT, offsetof(Techniques::GlobalTransformConstants, _worldSpaceView), 0 }
    };

    size_t GlobalTransform_ElementsCount = dimof(GlobalTransform_Elements);

        ////////////////////////////////////////////////////////////

    Metal::ConstantBufferLayoutElement LocalTransform_Elements[] = {
        { "LocalToWorld",                   Metal::NativeFormat::Matrix3x4,        offsetof(Techniques::LocalTransformConstants, _localToWorld), 0      },
        { "LocalSpaceView",                 Metal::NativeFormat::R32G32B32_FLOAT,  offsetof(Techniques::LocalTransformConstants, _localSpaceView), 0    }
    };

    size_t LocalTransform_ElementsCount = dimof(LocalTransform_Elements);

        ///////////////////////////////////////////////////////

    static void MakeIndentBuffer(char buffer[], unsigned bufferSize, signed identLevel)
    {
        std::fill(buffer, &buffer[std::min(std::max(0,identLevel), signed(bufferSize-1))], ' ');
        buffer[std::min(std::max(0,identLevel), signed(bufferSize-1))] = '\0';
    }

    void TraceTransformationMachine(
            std::ostream&   stream,
            const uint32*   commandStreamBegin,
            const uint32*   commandStreamEnd)
    {
        stream << "Transformation machine size: (" << commandStreamEnd - commandStreamBegin << ") bytes" << std::endl;

        char indentBuffer[32];
        signed indentLevel = 2;
        MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);

        for (auto i=commandStreamBegin; i!=commandStreamEnd;) {
            auto commandIndex = *i++;
            switch (commandIndex) {
            case TransformStackCommand::PushLocalToWorld:
                stream << indentBuffer << "PushLocalToWorld" << std::endl;
                ++indentLevel;
                MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                break;

            case TransformStackCommand::PopLocalToWorld:
                {
                    auto popCount = *i++;
                    stream << indentBuffer << "PopLocalToWorld (" << popCount << ")" << std::endl;
                    indentLevel -= popCount;
                    MakeIndentBuffer(indentBuffer, dimof(indentBuffer), indentLevel);
                }
                break;

            case TransformStackCommand::TransformFloat4x4_Static:
                {
                    auto trans = *reinterpret_cast<const Float4x4*>(AsPointer(i));
                    stream << indentBuffer << "TransformFloat4x4_Static (diag:" 
                        << trans(0,0) << ", " << trans(1,1) << ", " << trans(2,2) << ", " << trans(3,3) << ")" << std::endl;
                    i += 16;
                }
                break;

            case TransformStackCommand::Translate_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    stream << indentBuffer << "Translate_Static (" << trans[0] << ", " << trans[1] << ", " << trans[2] << ")" << std::endl;
                    i += 3;
                }
                break;

            case TransformStackCommand::RotateX_Static:
                stream << indentBuffer << "RotateX_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateY_Static:
                stream << indentBuffer << "RotateY_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateZ_Static:
                stream << indentBuffer << "RotateZ_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::Rotate_Static:
                {
                    auto a = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    float r = *reinterpret_cast<const float*>(AsPointer(i+3));
                    stream << indentBuffer << "Rotate_Static (" << a[0] << ", " << a[1] << ", " << a[2] << ")(" << r << ")" << std::endl;
                    i += 4;
                }
                break;

            case TransformStackCommand::UniformScale_Static:
                stream << indentBuffer << "UniformScale_Static (" << *reinterpret_cast<const float*>(AsPointer(i)) << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Static:
                {
                    auto trans = AsFloat3(reinterpret_cast<const float*>(AsPointer(i)));
                    stream << indentBuffer << "ArbitraryScale_Static (" << trans[0] << ", " << trans[1] << ", " << trans[2] << ")" << std::endl;
                }
                i+=3;
                break;

            case TransformStackCommand::TransformFloat4x4_Parameter:
                stream << indentBuffer << "TransformFloat4x4_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::Translate_Parameter:
                stream << indentBuffer << "Translate_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateX_Parameter:
                stream << indentBuffer << "RotateX_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateY_Parameter:
                stream << indentBuffer << "RotateY_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::RotateZ_Parameter:
                stream << indentBuffer << "RotateZ_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::Rotate_Parameter:
                stream << indentBuffer << "Rotate_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::UniformScale_Parameter:
                stream << indentBuffer << "UniformScale_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Parameter:
                stream << indentBuffer << "ArbitraryScale_Parameter (" << *i << ")" << std::endl;
                i++;
                break;

            case TransformStackCommand::WriteOutputMatrix:
                stream << indentBuffer << "WriteOutputMatrix (" << *i << ")" << std::endl;
                i++;
                break;
            }

            assert(i <= commandStreamEnd);  // make sure we haven't jumped past the end marker
        }
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
        ::Serialize(outputSerializer, _float4x4Parameters);
        ::Serialize(outputSerializer, _float4Parameters);
        ::Serialize(outputSerializer, _float3Parameters);
        ::Serialize(outputSerializer, _float1Parameters);
    }

        ////////////////////////////////////////////////////////////

    std::ostream& StreamOperator(std::ostream& stream, const GeoInputAssembly& ia)
    {
        stream << "Stride: " << ia._vertexStride << ": ";
        for (size_t c=0; c<ia._elements.size(); c++) {
            if (c != 0) stream << ", ";
            const auto& e = ia._elements[c];
            stream << e._semanticName << "[" << e._semanticIndex << "] " << Metal::AsString((Metal::NativeFormat::Enum)e._nativeFormat);
        }
        return stream;
    }

    std::ostream& StreamOperator(std::ostream& stream, const DrawCallDesc& dc)
    {
        return stream << "Mat: " << dc._subMaterialIndex << ", DrawIndexed(" << dc._indexCount << ", " << dc._firstIndex << ", " << dc._firstVertex << ")";
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
            //  committing the results to disk on exit
        auto cache = std::make_unique<MemoryMappedFile>(
            "int/TextureFormatCache.dat", entrySize * MaxCachedTextures + sizeof(Header),
            MemoryMappedFile::Access::Read|MemoryMappedFile::Access::Write|MemoryMappedFile::Access::OpenAlways);
        _cache = std::move(cache);
    }

    CachedTextureFormats::~CachedTextureFormats() {}

    bool IsDXTNormalMap(const std::string& textureName)
    {
        if (textureName.empty()) return false;

        auto& cache = Techniques::FindCachedBox<CachedTextureFormats>(
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
                i->second = (uint32)DeferredShaderResource::LoadFormat(textureName.c_str());
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                i->second = RenderCore::Metal::NativeFormat::Unknown;
            } CATCH_END
            ++hdr._count;
            return i->second >= RenderCore::Metal::NativeFormat::BC1_TYPELESS
                && i->second <= RenderCore::Metal::NativeFormat::BC1_UNORM_SRGB;
        }

        return      i->second >= RenderCore::Metal::NativeFormat::BC1_TYPELESS
                &&  i->second <= RenderCore::Metal::NativeFormat::BC1_UNORM_SRGB;
    }

    GeoInputAssembly CreateGeoInputAssembly(   
        const std::vector<Metal::InputElementDesc>& vertexInputLayout,
        unsigned vertexStride)
    { 
        GeoInputAssembly result;
        result._vertexStride = vertexStride;
        result._elements.reserve(vertexInputLayout.size());
        for (auto i=vertexInputLayout.begin(); i!=vertexInputLayout.end(); ++i) {
            RenderCore::Assets::VertexElement ele;
            XlZeroMemory(ele);     // make sure unused space is 0
            XlCopyNString(ele._semanticName, AsPointer(i->_semanticName.begin()), i->_semanticName.size());
            ele._semanticName[dimof(ele._semanticName)-1] = '\0';
            ele._semanticIndex = i->_semanticIndex;
            ele._nativeFormat = i->_nativeFormat;
            ele._alignedByteOffset = i->_alignedByteOffset;
            result._elements.push_back(ele);
        }
        return std::move(result);
    }
}}

