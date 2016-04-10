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
#include "../Format.h"
#include "../Types.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/OutputStream.h"
#include "../../ConsoleRig/Log.h"
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
        { "WorldToClip", Format::Matrix4x4, offsetof(Techniques::GlobalTransformConstants, _worldToClip), 0 },
        { "FrustumCorners", Format::R32G32B32A32_FLOAT, offsetof(Techniques::GlobalTransformConstants, _frustumCorners), 4 },
        { "WorldSpaceView", Format::R32G32B32_FLOAT, offsetof(Techniques::GlobalTransformConstants, _worldSpaceView), 0 }
    };

    size_t GlobalTransform_ElementsCount = dimof(GlobalTransform_Elements);

        ////////////////////////////////////////////////////////////

    Metal::ConstantBufferLayoutElement LocalTransform_Elements[] = {
        { "LocalToWorld",                   Format::Matrix3x4,        offsetof(Techniques::LocalTransformConstants, _localToWorld), 0      },
        { "LocalSpaceView",                 Format::R32G32B32_FLOAT,  offsetof(Techniques::LocalTransformConstants, _localSpaceView), 0    }
    };

    size_t LocalTransform_ElementsCount = dimof(LocalTransform_Elements);

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
            stream << e._semanticName << "[" << e._semanticIndex << "] " << AsString(e._nativeFormat);
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

        typedef std::pair<uint64, Format> Entry;
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

    static bool IsDXTNormalMap(Format format)
    {
        return unsigned(format) >= unsigned(RenderCore::Format::BC1_TYPELESS)
            && unsigned(format) <= unsigned(RenderCore::Format::BC1_UNORM_SRGB);
    }

    bool IsDXTNormalMap(const std::string& textureName)
    {
        if (textureName.empty()) return false;

        auto& cache = Techniques::FindCachedBox<CachedTextureFormats>(
            CachedTextureFormats::Desc());

        typedef CachedTextureFormats::Header Hdr;
        typedef CachedTextureFormats::Entry Entry;
        auto* data = cache._cache->GetData();
        if (!data) {
            static bool firstTime = true;
            if (firstTime) {
                LogAlwaysError << "Failed to open TextureFormatCache.dat! DXT normal map queries will be inefficient.";
                firstTime = false;
            }
            return IsDXTNormalMap(DeferredShaderResource::LoadFormat(textureName.c_str()));
        }

        auto& hdr = *(Hdr*)data;
        auto* start = (Entry*)PtrAdd(data, sizeof(Hdr));
        auto* end = (Entry*)PtrAdd(data, sizeof(Hdr) + sizeof(Entry) * hdr._count);

        auto hashName = Hash64(textureName);
        auto* i = std::lower_bound(start, end, hashName, CompareFirst<uint64, Format>());
        if (i == end || i->first != hashName) {
            if ((hdr._count+1) > CachedTextureFormats::MaxCachedTextures) {
                assert(0);  // cache has gotten too big
                return false;
            }

            std::move_backward(i, end, end+1);
            i->first = hashName;
            TRY {
                i->second = DeferredShaderResource::LoadFormat(textureName.c_str());
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                i->second = Format::Unknown;
            } CATCH_END
            ++hdr._count;
            return IsDXTNormalMap(i->second);
        }

        return IsDXTNormalMap(i->second);
    }

    GeoInputAssembly CreateGeoInputAssembly(   
        const std::vector<InputElementDesc>& vertexInputLayout,
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    ParameterBox TechParams_SetResHas(
        const ParameterBox& inputMatParameters, const ParameterBox& resBindings,
        const ::Assets::DirectorySearchRules& searchRules)
    {
        static const auto DefaultNormalsTextureBindingHash = ParameterBox::MakeParameterNameHash("NormalsTexture");
            // The "material parameters" ParameterBox should contain some "RES_HAS_..."
            // settings. These tell the shader what resource bindings are available
            // (and what are missing). We need to set these parameters according to our
            // binding list
        ParameterBox result = inputMatParameters;
        for (auto param=resBindings.Begin(); !param.IsEnd(); ++param) {
            result.SetParameter(StringMeld<64, utf8>() << "RES_HAS_" << param.Name(), 1);
            if (param.HashName() == DefaultNormalsTextureBindingHash) {
                auto resourceName = resBindings.GetString<::Assets::ResChar>(DefaultNormalsTextureBindingHash);
                ::Assets::ResChar resolvedName[MaxPath];
                searchRules.ResolveFile(resolvedName, dimof(resolvedName), resourceName.c_str());
                result.SetParameter(
                    (const utf8*)"RES_HAS_NormalsTexture_DXT", 
                    RenderCore::Assets::IsDXTNormalMap(resolvedName));
            }
        }
        return std::move(result);
    }
}}

