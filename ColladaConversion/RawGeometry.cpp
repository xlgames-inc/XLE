// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "../Core/Prefix.h"
#include "RawGeometry.h"
#include "ColladaConversion.h"
#include "../Assets/BlockSerializer.h"
#include "../RenderCore/RenderUtils.h"


#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADASaxFWLInputUnshared.h>
    #include <COLLADAFWMeshPrimitive.h>
    #include <COLLADAFWMeshVertexData.h>
    #include <COLLADAFWMesh.h>
    #include <COLLADAFWMeshPrimitiveWithFaceVertexCount.h>
    #include <COLLADAFWPolygons.h>
#pragma warning(pop)

#pragma warning(disable:4127)       // C4127: conditional expression is constant


namespace Serialization
{
    void Serialize( NascentBlockSerializer& serializer, 
                    const RenderCore::Metal::InputElementDesc&  object)
    {
        Serialize(serializer, object._semanticName);
        Serialize(serializer, object._semanticIndex);
        Serialize(serializer, unsigned(object._nativeFormat));
        Serialize(serializer, object._inputSlot);
        Serialize(serializer, object._alignedByteOffset);
        Serialize(serializer, unsigned(object._inputSlotClass));
        Serialize(serializer, object._instanceDataStepRate);
    }
}

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    class VertexAttribute 
    { 
    public:
        COLLADASaxFWL::InputSemantic::Semantic _basicSemantic;
        unsigned        _index;
        std::string     _semanticName;
        std::string     _sourceName;
        bool            _doTextureCoordinateFlip;

        VertexAttribute(COLLADASaxFWL::InputSemantic::Semantic basicSemantic, unsigned index, 
                        const std::string& semanticName, const std::string& sourceName, bool doTextureCoordinateFlip)
        :   _basicSemantic(basicSemantic), _index(index)
        ,   _semanticName(semanticName), _sourceName(sourceName)
        ,   _doTextureCoordinateFlip(doTextureCoordinateFlip)
        {}
    };

    namespace Operators
    {
        bool operator==(const VertexAttribute& lhs, const VertexAttribute &rhs)
        {
            if (rhs._index != lhs._index) return false;
            if (rhs._sourceName != lhs._sourceName) return false;

            if (lhs._basicSemantic == ~COLLADASaxFWL::InputSemantic::Semantic(0x0)) {
                return lhs._semanticName == rhs._semanticName;
            }
            return lhs._basicSemantic == rhs._basicSemantic;
        }

        bool operator!=(const VertexAttribute& lhs, const VertexAttribute &rhs)
        {
            return !operator==(lhs, rhs);
        }
    }
    using namespace Operators;

    std::vector<VertexAttribute> GetAttributeList(COLLADAFW::MeshPrimitive& primitive)
    {
        std::vector<VertexAttribute> result;

            //
            //       (assume everything has positions)
            //
            //      Note -- defining the preferred order here (as well as preferred semantics)
            //
            //          POSITION0
            //          NORMAL0
            //          TANGENT0
            //          BITANGET0           (not binormal)
            //          COLORX              (American spelling)
            //          TEXCOORDX
            //
            //          Can't seem to the get the "source" id for positions, normals, tangents and bitangents...?
            //
        result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::POSITION, 0, "POSITION", "Source-Position", false));
        if (primitive.hasNormalIndices()) {
            result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::NORMAL, 0, "NORMAL", "Source-Normals", false));
        }
        if (primitive.hasTangentIndices()) {
            result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::TANGENT, 0, "TANGENT", "Source-Tangents", false));
        }
        if (primitive.hasBinormalIndices()) {
            result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::BINORMAL, 0, "BITANGENT", "Source-Bitangents", false));
        }
        for (unsigned c=0; c<primitive.getColorIndicesArray().getCount(); c++) {
            assert(primitive.getColorIndices(c));
            result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::COLOR, c, "COLOR", primitive.getColorIndices(c)->getName(), false));
        }
        for (unsigned c=0; c<primitive.getUVCoordIndicesArray().getCount(); c++) {
            assert(primitive.getUVCoordIndices(c));
            result.push_back(VertexAttribute(COLLADASaxFWL::InputSemantic::TEXCOORD, c, "TEXCOORD", primitive.getUVCoordIndices(c)->getName(), true));
        }
        return result;
    }

    static size_t CreateTriangleWindingFromPolygon(size_t polygonVertexCount, unsigned buffer[], size_t bufferCount)
    {
            //
            //      Assuming simple convex polygon
            //      (nothing fancy required to convert to triangle list)
            //
        size_t outputIterator = 0;
        for (unsigned triangleCount = 0; triangleCount < polygonVertexCount - 2; ++triangleCount) {
                ////////        ////////
            unsigned v0, v1, v2;
            v0 = (triangleCount+1) / 2;
            if (triangleCount&0x1) {
                v1 = unsigned(polygonVertexCount - 2 - triangleCount/2);
            } else {
                v1 = unsigned(v0 + 1);
            }
            v2 = unsigned(polygonVertexCount - 1 - triangleCount/2);
                ////////        ////////
            buffer[outputIterator++] = v0;
            buffer[outputIterator++] = v1;
            buffer[outputIterator++] = v2;
                ////////        ////////
        }
        return outputIterator/3;
    }

    static const COLLADAFW::UIntValuesArray* Get(const COLLADAFW::MeshPrimitive& primitive, const VertexAttribute& attribute)
    {
        if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::POSITION) {
            return &primitive.getPositionIndices();
        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::NORMAL) {
            return &primitive.getNormalIndices();
        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::TANGENT) {
            return &primitive.getTangentIndices();
        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::BINORMAL) {
            return &primitive.getBinormalIndices();
        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::COLOR) {
            assert(attribute._index < primitive.getColorIndicesArray().getCount());
            return &primitive.getColorIndices(attribute._index)->getIndices();
        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::TEXCOORD) {
            assert(attribute._index < primitive.getUVCoordIndicesArray().getCount());
            return &primitive.getUVCoordIndices(attribute._index)->getIndices();
        }
        return nullptr;
    }

    class MeshVertexSourceData
    {
    public:
        class Param
        {
        public:
            enum Type { Float, Double };
            Type            _type;
            std::string     _name;
            Param(Type type, const std::string& name) : _type(type), _name(name) {}
        };
        const COLLADAFW::MeshVertexData*    _vertexData;
        std::string                         _name;
        size_t                              _start, _end, _stride;      // (in elements)
        std::vector<Param>                  _params;

        MeshVertexSourceData();

        static std::vector<Param>   DefaultPosition     (const COLLADAFW::MeshVertexData& vd, size_t stride);
        static std::vector<Param>   DefaultNormal       (const COLLADAFW::MeshVertexData& vd, size_t stride);
        static std::vector<Param>   DefaultTangent      (const COLLADAFW::MeshVertexData& vd, size_t stride);
        static std::vector<Param>   DefaultBitangent    (const COLLADAFW::MeshVertexData& vd, size_t stride);
        static std::vector<Param>   DefaultColor        (const COLLADAFW::MeshVertexData& vd, size_t stride);
        static std::vector<Param>   DefaultTexCoord     (const COLLADAFW::MeshVertexData& vd, size_t stride);
    };

    MeshVertexSourceData::MeshVertexSourceData()
    {
        _vertexData = nullptr;
        _start = _end = _stride = 0;
    }

    auto MeshVertexSourceData::DefaultPosition  (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>
    {
        std::vector<Param> result;
        auto type = 
            (vd.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT)
            ? MeshVertexSourceData::Param::Float
            : MeshVertexSourceData::Param::Double;
        const char* defaultNames[] = { "X", "Y", "Z", "W" };
        for (unsigned c=0; c<std::min(dimof(defaultNames), stride); ++c)
            result.push_back(MeshVertexSourceData::Param(type, defaultNames[c]));
        return std::move(result);
    }

    auto MeshVertexSourceData::DefaultNormal    (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>     { return DefaultPosition(vd, stride); }
    auto MeshVertexSourceData::DefaultTangent   (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>     { return DefaultPosition(vd, stride); }
    auto MeshVertexSourceData::DefaultBitangent (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>     { return DefaultPosition(vd, stride); }

    auto MeshVertexSourceData::DefaultColor     (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>
    {
        std::vector<Param> result;
        auto type = 
            (vd.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT)
            ? MeshVertexSourceData::Param::Float
            : MeshVertexSourceData::Param::Double;
        const char* defaultNames[] = { "R", "G", "B", "A", "A2", "A3", "A4", "A5", "A6", "A7", "A8" };
        for (unsigned c=0; c<std::min(dimof(defaultNames), stride); ++c)
            result.push_back(MeshVertexSourceData::Param(type, defaultNames[c]));
        return std::move(result);
    }

    auto MeshVertexSourceData::DefaultTexCoord  (const COLLADAFW::MeshVertexData& vd, size_t stride) -> std::vector<Param>
    {
        std::vector<Param> result;
        auto type = 
            (vd.getType() == COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT)
            ? MeshVertexSourceData::Param::Float
            : MeshVertexSourceData::Param::Double;
        const char* defaultNames[] = { "S", "T", "U", "V", "W", "X", "Y", "Z" };
        for (unsigned c=0; c<std::min(dimof(defaultNames), stride); ++c)
            result.push_back(MeshVertexSourceData::Param(type, defaultNames[c]));
        return std::move(result);
    }

    static MeshVertexSourceData GetVertexData(const COLLADAFW::Mesh& primitive, const VertexAttribute& attribute)
    {
        using namespace COLLADAFW;
        if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::POSITION) {

                //
                //      FWCollada removes a lot of information that's present in the
                //      collada file.
                //
                //      Let's try to reconstruct the <accessor> part!
                //
            auto& vertexData        = primitive.getPositions();
            MeshVertexSourceData result;
            result._vertexData      = &vertexData;
            result._start           = 0;
            if (vertexData.getNumInputInfos() == 0) {
                result._name        = std::string();
                result._end         = vertexData.getFloatValues()->getCount();
                result._stride      = 3;
            } else {
                assert(vertexData.getNumInputInfos()==1);
                assert(vertexData.getInputInfosArray()[0]->mName == attribute._sourceName);
                result._name        = vertexData.getInputInfosArray()[0]->mName;
                result._end         = vertexData.getInputInfosArray()[0]->mLength;
                result._stride      = vertexData.getInputInfosArray()[0]->mStride;
            }
            result._params      = MeshVertexSourceData::DefaultPosition(vertexData, result._stride);
            return std::move(result);

        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::NORMAL) {

            auto& vertexData        = primitive.getNormals();
            MeshVertexSourceData result;
            result._vertexData      = &vertexData;
            result._start           = 0;
            if (vertexData.getNumInputInfos() == 0) {
                result._name        = std::string();
                result._end         = vertexData.getFloatValues()->getCount();
                result._stride      = 3;
            } else {
                assert(vertexData.getNumInputInfos()==1);
                assert(vertexData.getInputInfosArray()[0]->mName == attribute._sourceName);
                result._name        = vertexData.getInputInfosArray()[0]->mName;
                result._end         = vertexData.getInputInfosArray()[0]->mLength;
                result._stride      = vertexData.getInputInfosArray()[0]->mStride;
            }
            result._params      = MeshVertexSourceData::DefaultNormal(vertexData, result._stride);
            return std::move(result);

        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::TANGENT) {

            auto& vertexData        = primitive.getTangents();
            MeshVertexSourceData result;
            result._vertexData      = &vertexData;
            result._start           = 0;
            if (vertexData.getNumInputInfos() == 0) {
                result._name        = std::string();
                result._end         = vertexData.getFloatValues()->getCount();
                result._stride      = 3;
            } else {
                assert(vertexData.getNumInputInfos()==1);
                // assert(vertexData.getInputInfosArray()[0]->mName == attribute._sourceName); (this assert is incorrect, we shouldn't always see the same name here)
                result._name        = vertexData.getInputInfosArray()[0]->mName;
                result._end         = vertexData.getInputInfosArray()[0]->mLength;
                result._stride      = vertexData.getInputInfosArray()[0]->mStride;
            }
            result._params      = MeshVertexSourceData::DefaultTangent(vertexData, result._stride);
            return std::move(result);

        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::BINORMAL) {

            auto& vertexData        = primitive.getBinormals();
            MeshVertexSourceData result;
            result._vertexData      = &vertexData;
            result._start           = 0;
            if (vertexData.getNumInputInfos() == 0) {
                result._name        = std::string();
                result._end         = vertexData.getFloatValues()->getCount();
                result._stride      = 3;
            } else {
                assert(vertexData.getNumInputInfos()==1);
                // assert(vertexData.getInputInfosArray()[0]->mName == attribute._sourceName); (this assert is incorrect, we shouldn't always see the same name here)
                result._name        = vertexData.getInputInfosArray()[0]->mName;
                result._end         = vertexData.getInputInfosArray()[0]->mLength;
                result._stride      = vertexData.getInputInfosArray()[0]->mStride;
            }
            result._params      = MeshVertexSourceData::DefaultBitangent(vertexData, result._stride);
            return std::move(result);

        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::COLOR) {

            auto& vertexData    = primitive.getColors();
            size_t sourceIndex = ~size_t(0x0);
            size_t sourceOffset = 0;
            for (size_t index=0; index<vertexData.getValuesCount(); ++index) {
                if (COLLADABU::Utils::equals(attribute._sourceName, vertexData.getName(index))) {
                    sourceIndex = index; 
                    break;
                }
                sourceOffset += vertexData.getInputInfosArray()[index]->mLength;
            }
            if (sourceIndex == ~size_t(0x0)) {
                assert(0);      // mismatch! couldn't connect primitive input to source data
                sourceIndex = 0;
            }

            MeshVertexSourceData result;
            result._vertexData  = &vertexData;
            result._start       = sourceOffset;
            result._name        = vertexData.getInputInfosArray()[sourceIndex]->mName;
            result._end         = vertexData.getInputInfosArray()[sourceIndex]->mLength + sourceOffset;
            result._stride      = vertexData.getInputInfosArray()[sourceIndex]->mStride;
            result._params      = MeshVertexSourceData::DefaultColor(vertexData, result._stride);
            return std::move(result);

        } else if (attribute._basicSemantic == COLLADASaxFWL::InputSemantic::TEXCOORD) {

            auto& vertexData    = primitive.getUVCoords();
            size_t sourceIndex = ~size_t(0x0);
            size_t sourceOffset = 0;
            for (size_t index=0; index<vertexData.getValuesCount (); ++index) {
                if (COLLADABU::Utils::equals(attribute._sourceName, vertexData.getName(index))) {
                    sourceIndex = index; break;
                }
                sourceOffset += vertexData.getInputInfosArray()[index]->mLength;
            }
            if (sourceIndex == ~size_t(0x0)) {
                assert(0);      // mismatch! couldn't connect primitive input to source data
                sourceIndex = 0;
            }

            MeshVertexSourceData result;
            result._vertexData  = &vertexData;
            result._start       = sourceOffset;
            result._name        = vertexData.getInputInfosArray()[sourceIndex]->mName;
            result._end         = vertexData.getInputInfosArray()[sourceIndex]->mLength + sourceOffset;
            result._stride      = vertexData.getInputInfosArray()[sourceIndex]->mStride;
            result._params      = MeshVertexSourceData::DefaultTexCoord(vertexData, result._stride);
            return std::move(result);
        }
        return MeshVertexSourceData();
    }

    class DestinationFormat
    {
    public:
        enum ComponentType {
            Float32, Float16, UNorm8
        };
        ComponentType               _type;
        unsigned                    _componentCount;
        Metal::NativeFormat::Enum   _format;
            
        DestinationFormat(ComponentType type, unsigned componentCount, Metal::NativeFormat::Enum format) : _type(type), _componentCount(componentCount), _format(format) {}
        DestinationFormat() { _type = Float32; _componentCount = 0; _format = Metal::NativeFormat::Unknown; }
    };

    static DestinationFormat AsNativeFormat(const MeshVertexSourceData::Param parameters[], size_t parameterCount)
    {
        if (parameterCount == 0 || !parameters) {
            return DestinationFormat(DestinationFormat::Float32, 0, Metal::NativeFormat::Unknown);
        }

            //
            //      Calculate a native format that matches this source data.
            //      Actually, there are a limited number of relevant native formats.
            //      So, it's easy to find one that works.
            //
            //      We don't support doubles in vertex buffers. So we can only choose from
            //
            //          R32G32B32A32_FLOAT
            //          R32G32B32_FLOAT
            //          R32G32_FLOAT
            //          R32_FLOAT
            //
            //          (assuming R9G9B9E5_SHAREDEXP, etc, not valid for vertex buffers)
            //          R10G10B10A2_UNORM   (ok for DX 11.1 -- but DX11??)
            //          R10G10B10A2_UINT    (ok for DX 11.1 -- but DX11??)
            //          R11G11B10_FLOAT     (ok for DX 11.1 -- but DX11??)
            //
            //          R8G8B8A8_UNORM      (SRGB can't be used)
            //          R8G8_UNORM
            //          R8_UNORM
            //          B8G8R8A8_UNORM
            //          B8G8R8X8_UNORM
            //
            //          B5G6R5_UNORM        (on some hardware)
            //          B5G5R5A1_UNORM      (on some hardware)
            //          B4G4R4A4_UNORM      (on some hardware)
            //
            //          R16G16B16A16_FLOAT
            //          R16G16_FLOAT
            //          R16_FLOAT
            //
            //          (or UINT, SINT, UNORM, SNORM versions of the same thing)
            //

        if (parameters[0]._name == "R") {
                //  (assume this is vertex colour. Choose an 8 bit type)
                //  only supporting R, G, B, A order (not B, G, R, A order)
            if (parameterCount == 1)        return DestinationFormat(DestinationFormat::UNorm8, 1, Metal::NativeFormat::R8_UNORM);
            else if (parameterCount == 2)   return DestinationFormat(DestinationFormat::UNorm8, 2, Metal::NativeFormat::R8G8_UNORM);
            else                            return DestinationFormat(DestinationFormat::UNorm8, 4, Metal::NativeFormat::R8G8B8A8_UNORM);
        }
            
            //  OpenGL doesn't seem to support 16 bit floats in vertex buffers normally. 
            //  We have to use fixed point? 
            //      \todo --    this should be configurable. We need some why to specific
            //                  configuration settings for model conversion
        const bool use16BitFloats = true;
        if (use16BitFloats) {
            if (parameterCount == 1)        return DestinationFormat(DestinationFormat::Float16, 1, Metal::NativeFormat::R16_FLOAT);
            else if (parameterCount == 2)   return DestinationFormat(DestinationFormat::Float16, 2, Metal::NativeFormat::R16G16_FLOAT);
            else                            return DestinationFormat(DestinationFormat::Float16, 4, Metal::NativeFormat::R16G16B16A16_FLOAT);
        } else {
            if (parameterCount == 1)        return DestinationFormat(DestinationFormat::Float32, 1, Metal::NativeFormat::R32_FLOAT);
            else if (parameterCount == 2)   return DestinationFormat(DestinationFormat::Float32, 2, Metal::NativeFormat::R32G32_FLOAT);
            else if (parameterCount == 3)   return DestinationFormat(DestinationFormat::Float32, 3, Metal::NativeFormat::R32G32B32_FLOAT);
            else                            return DestinationFormat(DestinationFormat::Float32, 4, Metal::NativeFormat::R32G32B32A32_FLOAT);
        }
    }

    // static unsigned Get(const COLLADAFW::MeshPrimitive& primitive, const VertexAttribute& attribute, unsigned indexIntoPrimitive)
    // {
    //     auto indexArray = Get(primitive, attribute);
    //     if (!indexArray) {
    //         return ~unsigned(0x0);
    //     }
    //     assert(indexIntoPrimitive < indexArray->getCount());
    //     return (*indexArray)[indexIntoPrimitive];
    // }

    static unsigned Get(const COLLADAFW::MeshPrimitive& primitive, const COLLADAFW::UIntValuesArray* attribute, unsigned indexIntoPrimitive)
    {
        assert(indexIntoPrimitive < attribute->getCount());
        return (*attribute)[indexIntoPrimitive];
    }

    typedef std::vector<std::pair<unsigned, unsigned>> VertexHashTable;

    static size_t BuildUnifiedVertex( 
        std::vector<std::vector<unsigned>>& vertexMap,
        VertexHashTable* vertexHashTable,
        const std::vector<const COLLADAFW::UIntValuesArray*>& semantics,
        const COLLADAFW::MeshPrimitive& primitive, unsigned indexIntoPrimitive)
    {
        unsigned indexOfEachAttribute[32];
        if (semantics.size() > dimof(indexOfEachAttribute)) {
            ThrowException(FormatError("Exceeded maximum vertex semantics"));
        }

        unsigned hashKey = 0;
        auto c = 0u;
        for (auto i = semantics.cbegin(); i != semantics.cend(); ++i, ++c) {
            indexOfEachAttribute[c] = Get(primitive, *i, indexIntoPrimitive);
            hashKey += indexOfEachAttribute[c] << (4*std::distance(semantics.cbegin(), i));
        }
        unsigned attributeCount = (unsigned)semantics.size();

            //
            //      Now we have the index for each attribute
            //      Let's check if this vertex already exists in 
            //      
            //      Note that it's possible that we could come across a vertex that
            //      is identical except for the unused part. It this case we could
            //      re-purpose the vertex. It's an unlikely case, but it could
            //      happen.
            //
        size_t minSize = std::numeric_limits<size_t>::max();
        for (auto i = vertexMap.cbegin(); i!=vertexMap.cend(); ++i) {
            if (i!=vertexMap.cbegin()) { assert(i->size() == minSize); }    // check for missized arrays. They should all be the same length
            minSize = std::min(minSize, i->size());
        }

            //
            //      This test is pretty expensive! Use a hash table to speed it up.
            //      But this doesn't work well with incomplete vertices. If there
            //      are some incomplete vertices already written into the vertex buffer,
            //      they won't be correctly matched.
            //
            //      Another way to do this is to allow duplicates at this stage, but
            //      combine the duplicates after the full vertex buffer is created.
            //
            //      That would mean the intermediate buffers could grow to larger sizes;
            //      but the duplicate-removal would probably have lower overhead all up.
            //
        if (vertexHashTable) {

                //  
                //      Visual studio iterator debugging is too expensive for this lookup. It's *extremely* expensive.
                //          We can get around it by calling std::_Equal_range (but this might be not compatible
                //          with other versions of visual studio).
                //
                //          This will disable iterator debugging for this call only, and we can keep it on
                //          in other places.
                //
                //          Another option is to write a custom implementation of std::equal_range...
                //
            #if (STL_ACTIVE == STL_MSVC) && (_ITERATOR_DEBUG_LEVEL >= 2)
                auto range = std::_Equal_range(vertexHashTable->begin(), vertexHashTable->end(), hashKey, CompareFirst<unsigned, unsigned>(), 
                    _Dist_type(vertexHashTable->begin()));
            #else
                auto range = std::equal_range(vertexHashTable->begin(), vertexHashTable->end(), hashKey, CompareFirst<unsigned, unsigned>());
            #endif
                
            for (auto i=range.first; i!=range.second; ++i) {
                size_t testingVertex = i->second;
                auto a = 0u;
                for (; a<attributeCount; ++a) {
                    if (indexOfEachAttribute[a] != vertexMap[a][testingVertex]) {
                        break;
                    }
                }
                if (a==attributeCount) {
                    return testingVertex;
                }
            }

                //
                //      We didn't find it; so push in the key for the vertex
                //      we're about to add
                //
            vertexHashTable->insert(range.first, std::make_pair(hashKey, unsigned(minSize)));
        } else {
            for (size_t testingVertex=0; testingVertex<minSize; ++testingVertex) {
                auto a = 0u;
                for (; a<attributeCount; ++a) {
                    if (indexOfEachAttribute[a] != vertexMap[a][testingVertex]
                        && vertexMap[a][testingVertex] != ~unsigned(0x0)) {
                        break;
                    }
                }
                if (a==attributeCount) {
                        //  hit end point; we got a match!
                        //  Write all of the properties again, in case we're re-purposing an incomplete vertex
                    for (auto a=0u; a<attributeCount; ++a) {
                        vertexMap[a][testingVertex] = indexOfEachAttribute[a];
                    }
                    return testingVertex;
                }
            }
        }

            //      We didn't find it. We need to create a new vertex and pop it onto the
            //      end of the list.
        for (auto a=0u; a<attributeCount; ++a) {
            vertexMap[a].push_back(indexOfEachAttribute[a]);
        }
        return minSize;
    }

    static const char* AsString(COLLADAFW::Geometry::GeometryType type) 
    {
        switch (type) {
        case COLLADAFW::Geometry::GEO_TYPE_MESH:            return "Mesh";
        case COLLADAFW::Geometry::GEO_TYPE_SPLINE:          return "Spline";
        case COLLADAFW::Geometry::GEO_TYPE_CONVEX_MESH:     return "Convex Mesh";
        }
        return "<<unknown>>";
    }

    static const char* AsString(COLLADAFW::MeshPrimitive::PrimitiveType type)
    {
        switch (type) {
        case COLLADAFW::MeshPrimitive::LINES: return "Lines";
        case COLLADAFW::MeshPrimitive::LINE_STRIPS: return "Line Strips";
        case COLLADAFW::MeshPrimitive::POLYGONS: return "Polygons";
        case COLLADAFW::MeshPrimitive::POLYLIST: return "Polylist";
        case COLLADAFW::MeshPrimitive::TRIANGLES: return "Triangles";
        case COLLADAFW::MeshPrimitive::TRIANGLE_FANS: return "TriangleFans";
        case COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS: return "TriangleStrips";
        case COLLADAFW::MeshPrimitive::POINTS: return "Points";
        }
        return "<<unknown>>";
    }

    NascentRawGeometry Convert(const COLLADAFW::Geometry* geometry)
    {
            //  
            //      We can only handle "mesh" data inside a geometry element.
            //      Collada also support higher-order surfaces, but these aren't
            //      supported by us currently.
            //
        if (geometry->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH) {
            ThrowException(FormatError(
                "Failed while importing geometry element (%s).\r Non mesh type encountered (%s). This object must be converted into a mesh in the exporter tool.",
                geometry->getName().c_str(), AsString(geometry->getType())));
        }

            //
            //      Note that "COLLADAFW" only support the semantics in the common
            //      profile. To support arbitrary semantics, the best way is to
            //      modify COLLADAFWMesh.cpp to expand support for more different
            //      semantics. (actually, this should be pretty easy to do -- but
            //      it could help to cleanup the file a bit while doing it)
            //
            //      For flexibility, let's use strings rather than enums for 
            //      semantics while processing.
            //
        const COLLADAFW::Mesh* mesh = COLLADAFW::objectSafeCast<COLLADAFW::Mesh>(geometry);
        if (!mesh) {
            ThrowException(FormatError("Casting failure while processing geometry node (%s).", geometry->getName().c_str()));
        }

            // some exports can have empty meshes -- ideally, we just want to ignore them
        if (!mesh->getFacesCount() || mesh->getPositions().empty()) {
            return NascentRawGeometry();
        }

            //
            //      In Collada format, we have a separate index buffer per input
            //      attribute. 
            //
            //      This actually looks like it works very well; some attributes 
            //      have much higher sharing than others.
            //
            //      But for modern graphics hardware, we can only support a single
            //      index buffer (and each vertex is a fixed combination of all
            //      attributes). So during conversion we will switch to the GPU friendly
            //      format (let's call this the "unified" vertex format).
            //
            //      We also need to convert from various input primitives to primitives
            //      we can work with. 
            //
            //      Input primitives:
            //          lines           -> line list
            //          linestrips      -> line strip
            //          polygons        -> triangle list
            //          polylist        -> triangle list
            //          triangles       -> triangle list
            //          trifans         -> triangle fan (with terminator indices)
            //          tristrips       -> triangle strip (with terminator indices)
            //
            //      Note that we should do some geometry optimisation after conversion
            //      (because the raw output from our tools may not be optimal)
            //
            //      This entire mesh should collapse to a single vertex buffer and index
            //      buffer. In some cases there may be an advantage to using multiple
            //      vertex buffers (for example, if some primitives use fewer vertex 
            //      attributes than others). Let's just ignore this for the moment.
            //
            //      Sometimes this geometry will use multiple different materials.
            //      Even when this happens, we'll keep using the same VB/IB. We'll
            //      just use separate draw commands for each material.
            //  

        std::vector<VertexAttribute>        vertexSemantics;
        typedef std::vector<unsigned>       PendingIndexBuffer;
        std::vector<PendingIndexBuffer>     vertexMap;
        VertexHashTable                     vertexHashTable;

        size_t unifiedVertexCountGuess = 0;
        {
                //
                //  Guess the max unified vertex count; and reserve our space.
                //
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getPositions().getValuesCount());
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getNormals().getValuesCount());
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getColors().getValuesCount());
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getUVCoords().getValuesCount());
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getTangents().getValuesCount());
            unifiedVertexCountGuess = std::max(unifiedVertexCountGuess, mesh->getBinormals().getValuesCount());
            vertexHashTable.reserve(unifiedVertexCountGuess);
        }

        std::vector<const COLLADAFW::UIntValuesArray*>  vertexAttributes;

        class DrawOperation
        {
        public:
            std::vector<unsigned>   _indexBuffer;
            Metal::Topology::Enum   _topology;
            COLLADAFW::MaterialId   _materialId;
        };
        std::vector<DrawOperation>      drawOperations;

        using namespace COLLADAFW;


            //
            //      First; deal with index buffers and draw calls
            //
        const MeshPrimitiveArray& meshPrimitives = mesh->getMeshPrimitives();
        for (size_t c=0; c<meshPrimitives.getCount(); ++c) {

                //  Ignore anything using a material called "proxy"
                //  this is sometimes used for secondary models stored inside of the main model
                //  (for example, for a physical proxy)
            if (!XlCompareStringI(meshPrimitives[c]->getMaterial().c_str(), "proxy")) {
                continue;
            }

                //
                //      First, set up the vertices so we know where and how to write.
                //      If we need to add new attributes, we should do so now.
                //
                //      First, convert from the COLLADAFW::MeshPrimitive scheme of
                //      separate accessors for each semantic, to something more generic
                //
            std::vector<VertexAttribute> primitiveAttributes = GetAttributeList(*meshPrimitives[c]);

            auto localIterator = primitiveAttributes.begin();
            auto globalIterator = vertexSemantics.begin();
            while (localIterator != primitiveAttributes.end()) {
                if (globalIterator == vertexSemantics.end() || *localIterator != *globalIterator) {
                    size_t insertLocation = std::distance(vertexSemantics.begin(), globalIterator);
                    globalIterator = vertexSemantics.insert(globalIterator, *localIterator) + 1;
                    if (!vertexMap.empty()) {
                            //  insert a new array the same length as all the others
                            //  (it will have invalid entries in the spaces where it wasn't
                            //  required).
                        PendingIndexBuffer ib(vertexMap[0].size(), ~PendingIndexBuffer::value_type(0x0));
                        ib.reserve(unifiedVertexCountGuess);
                        vertexMap.insert(
                            vertexMap.begin() + insertLocation, 
                            std::move(ib));
                    } else {
                        PendingIndexBuffer ib;
                        ib.reserve(unifiedVertexCountGuess);
                        vertexMap.push_back(std::move(ib));
                    }
                    ++localIterator;
                } else {
                    ++globalIterator;
                    ++localIterator;
                }
            }

                // pre-calculate which vertex attribute lists match which semantics
            vertexAttributes.erase(vertexAttributes.begin(), vertexAttributes.end());   // empty without deallocation
            vertexAttributes.reserve(vertexSemantics.size());
            for (auto i = vertexSemantics.cbegin(); i != vertexSemantics.cend(); ++i) {
                vertexAttributes.push_back(Get(*meshPrimitives[c], *i));
            }
                
            DrawOperation convertDrawCall;
            if (!vertexAttributes.empty()) {
                convertDrawCall._indexBuffer.reserve(vertexAttributes[0]->getCount());      // reserve to some conservative large number
            }

            convertDrawCall._materialId = meshPrimitives[c]->getMaterialId();

            const MeshPrimitive::PrimitiveType primitiveType = meshPrimitives[c]->getPrimitiveType();
            if (primitiveType == MeshPrimitive::POLYGONS || primitiveType == MeshPrimitive::POLYLIST) {
                    
                convertDrawCall._topology = Metal::Topology::TriangleList;     // (will become a triangle list.. perhaps some hardware can support convex polygons...?)

                    // A list of polygons. 
                const MeshPrimitiveWithFaceVertexCount<int>* polygons = 
                    COLLADAFW::objectSafeCast<const MeshPrimitiveWithFaceVertexCount<int>>(meshPrimitives[c]);
                if (!polygons) {
                    ThrowException(FormatError("Casting failure while processing geometry node (%s).", geometry->getName().c_str()));
                }

                const Polygons::VertexCountArray& vertexCounts = polygons->getGroupedVerticesVertexCountArray();
                size_t groupStart = 0;
                for (auto group=0u; group<vertexCounts.getCount(); ++group) {
                    size_t groupEnd = groupStart + vertexCounts[group];

                    const unsigned MaxPolygonSize = 32;
                    if ((groupEnd - groupStart) > MaxPolygonSize) {
                        ThrowException(FormatError("Exceeded maximum polygon size in node (%s).", geometry->getName().c_str()));
                    }
                    if ((groupEnd - groupStart) <=2) {
                        ThrowException(FormatError("Polygon with less than 3 vertices in node (%s).", geometry->getName().c_str()));
                    }

                        //
                        //      Convert this polygon into a triangle list and
                        //      create unified vertices for it.
                        //
                    size_t unifiedVertices[MaxPolygonSize];
                    for (auto v=groupStart; v!=groupEnd; ++v) {
                        unifiedVertices[v-groupStart] = BuildUnifiedVertex(
                            vertexMap, &vertexHashTable, vertexAttributes, *polygons, (unsigned)v);
                    }

                    unsigned triangleWinding[MaxPolygonSize+2];
                    size_t triangleCount = CreateTriangleWindingFromPolygon(
                        groupEnd - groupStart, triangleWinding, dimof(triangleWinding));

                    for (auto v=0u; v<triangleCount*3; ++v) {
                        assert(triangleWinding[v] < (groupEnd-groupStart));
                        convertDrawCall._indexBuffer.push_back(
                            (unsigned)unifiedVertices[triangleWinding[v]]);
                    }

                    groupStart = groupEnd;
                }
            } else if (primitiveType == MeshPrimitive::TRIANGLES) {

                    //  Triangle list -> triangle list (simpliest conversion)
                convertDrawCall._topology = Metal::Topology::TriangleList;     // (will become a triangle list.. perhaps some hardware can support convex polygons...?)
                for (auto index=0u; index<meshPrimitives[c]->getFaceCount()*3; ++index) {
                    convertDrawCall._indexBuffer.push_back( 
                        (unsigned)BuildUnifiedVertex(
                            vertexMap, &vertexHashTable, vertexAttributes, *meshPrimitives[c], index));
                }
            } else {
                ThrowException(FormatError("Unsupported primitive type found in mesh (%s) (%s)", mesh->getName().c_str(), AsString(primitiveType)));
            }

            drawOperations.push_back(std::move(convertDrawCall));
        }

            // if we didn't end up with any valid draw calls, we need to return a blank object
        if (drawOperations.empty()) {
            return NascentRawGeometry();
        }

            //
            //      Now, deal with vertex buffers
            //
            //      We should have a list of unified vertices, and the index of each attribute
            //      for them. Let's pull that together into something that looks more like a 
            //      vertex buffer.
            //
            //      We can select whether to use interleaved vertex buffers here; or separate
            //      each attribute. Usually interleaved should be better; but some rare cases
            //      could benefit from custom ordering. Let's build the input layout first, and
            //      use that to determine how we write the vertices into our nascent vertex buffer.
            //

        auto meshVertexSourceData    = std::make_unique<MeshVertexSourceData[]>(vertexAttributes.size());
        auto destinationFormats      = std::make_unique<DestinationFormat[]>(vertexAttributes.size());

        std::vector<Metal::InputElementDesc> nativeElements(vertexAttributes.size());
        size_t vertexSize, vertexCount;
        {
            size_t accumulatingOffset = 0;
            for (auto i=vertexSemantics.cbegin(); i!=vertexSemantics.cend(); ++i) {
                auto& nativeElement  = nativeElements[std::distance(vertexSemantics.cbegin(), i)];
                auto& sourceData     = meshVertexSourceData[std::distance(vertexSemantics.cbegin(), i)];
                sourceData = GetVertexData(*mesh, *i);

                    // Note --  There's a problem here with texture coordinates. Sometimes texture coordinates
                    //          have 3 components in the Collada file. But only 2 components are actually used
                    //          by mapping. The last component might just be redundant. The only way to know 
                    //          for sure that the final component is redundant is to look at where the geometry
                    //          is used, and how this vertex element is bound to materials. But in this function
                    //          call we only have access to the "Geometry" object, without any context information.
                    //          We don't yet know how it will be bound to materials.
                destinationFormats[std::distance(vertexSemantics.cbegin(), i)] = AsNativeFormat(AsPointer(sourceData._params.begin()), sourceData._params.size());

                nativeElement._semanticName         = i->_semanticName;
                nativeElement._semanticIndex        = i->_index;
                nativeElement._nativeFormat         = destinationFormats[std::distance(vertexSemantics.cbegin(), i)]._format;
                nativeElement._inputSlot            = 0;
                nativeElement._alignedByteOffset    = (unsigned)accumulatingOffset;
                nativeElement._inputSlotClass       = Metal::InputClassification::PerVertex;
                nativeElement._instanceDataStepRate = 0;

                accumulatingOffset += Metal::BitsPerPixel(nativeElement._nativeFormat)/8;
            }

            vertexSize = accumulatingOffset;
            vertexCount = std::numeric_limits<size_t>::max();
            for (auto i = vertexMap.cbegin(); i!=vertexMap.cend(); ++i) {
                if (i!=vertexMap.cbegin()) { assert(i->size() == vertexCount); }    // check for missized arrays. They should all be the same length
                vertexCount = std::min(vertexCount, i->size());
            }
        }

            //
            //      Write the data into the vertex buffer
            //
        const unsigned short half0 = AsFloat16(0.f);
        const unsigned short half1 = AsFloat16(1.f);
        auto finalVertexBuffer = std::make_unique<uint8[]>(vertexSize*vertexCount);
        auto unifiedVertexIndexToPositionIndex = std::make_unique<uint32[]>(vertexCount);

        for (auto i=vertexSemantics.cbegin(); i!=vertexSemantics.cend(); ++i) {
            auto semanticIndex            = std::distance(vertexSemantics.cbegin(), i);
            const auto& nativeElement     = nativeElements[semanticIndex];
            const auto& sourceData        = meshVertexSourceData[semanticIndex];
            const auto& destinationFormat = destinationFormats[semanticIndex];

            if (sourceData._params[0]._type == MeshVertexSourceData::Param::Float) {

                    //      This could be be made more efficient with a smarter loop..
                for (size_t v=0; v<vertexCount; ++v) {
                    auto vertexDestination = &finalVertexBuffer.get()[v*vertexSize];
                    auto attributeIndex = vertexMap[semanticIndex][v];

                        //      Collada has this idea of "vertex index"; which is used to map
                        //      on the vertex weight information. But that seems to be lost in OpenCollada.
                        //      All we can do is use the position index as a subtitute.
                    if (semanticIndex == 0) {
                        assert(i->_basicSemantic == COLLADASaxFWL::InputSemantic::POSITION);    // assuming the first is position, for simplicity
                        assert(attributeIndex < mesh->getPositions().getValuesCount());
                        unifiedVertexIndexToPositionIndex[v] = (uint32)attributeIndex;
                    }

                        //
                        //      Input is float data.
                        //          output maybe float, float16 -- or maybe UNORM type...?
                        //
                        //      note that the "sourceData._start" offset is already included
                        //      into the attributeIndex value, so we don't have to add it again
                        //
                    auto sourceStart    = &sourceData._vertexData->getFloatValues()->getData()
                        [/*sourceData._start +*/ attributeIndex * sourceData._stride];
                    auto destination    = PtrAdd(vertexDestination, nativeElement._alignedByteOffset);

                    if (destinationFormat._type == DestinationFormat::Float32) {

                        for (unsigned c=0; c<destinationFormat._componentCount; ++c) {
                            if (c < sourceData._stride) {
                                ((float*)destination)[c] = ((float*)sourceStart)[c];
                            } else {
                                ((float*)destination)[c] = (c < 3)?0.f:1.f; // default for values not set in Collada
                            }

                            if (i->_doTextureCoordinateFlip && destinationFormat._componentCount >= 2) {
                                ((float*)destination)[1] = 1.0f - ((float*)sourceStart)[1];
                            }
                        }

                    } else if (destinationFormat._type == DestinationFormat::Float16) {

                        for (unsigned c=0; c<destinationFormat._componentCount; ++c) {
                            if (c < sourceData._stride) {
                                ((unsigned short*)destination)[c] = AsFloat16(((float*)sourceStart)[c]);
                            } else {
                                ((unsigned short*)destination)[c] = (c < 3)?half0:half1;    // default for values not set in Collada
                            }
                        }

                        if (i->_doTextureCoordinateFlip && destinationFormat._componentCount >= 2) {
                            ((unsigned short*)destination)[1] = AsFloat16(1.0f - ((float*)sourceStart)[1]);
                        }

                    } else if (destinationFormat._type == DestinationFormat::UNorm8) {

                        for (unsigned c=0; c<destinationFormat._componentCount; ++c) {
                            if (c < sourceData._stride) {
                                ((unsigned char*)destination)[c] = (unsigned char)Clamp(((float*)sourceStart)[c]*255.f, 0.f, 255.f);
                            } else {
                                ((unsigned char*)destination)[c] = (c < 3)?0x0:0xff;    // default for values not set in Collada
                            }
                        }

                        if (i->_doTextureCoordinateFlip && destinationFormat._componentCount >= 2) {
                            auto t = 1.0f - ((float*)sourceStart)[1];
                            ((unsigned char*)destination)[1] = (unsigned char)(std::max(0.f, std::max(1.f, t/255.f)));
                        }

                    }
                }
            }
        }

            //
            //      Write data into the index buffer. Note we can select 16 bit or 32 bit index buffer
            //      here. Most of the time 16 bit should be enough (but sometimes we need 32 bits)
            //
            //      All primitives should go into the same index buffer. They we record all of the 
            //      separate draw calls.
            //
            //      The end result is 1 vertex buffer and 1 index buffer.
            //

        std::vector<NascentDrawCallDesc> finalDrawOperations;
        finalDrawOperations.reserve(drawOperations.size());

        std::vector<uint64> materialIds;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
            uint64 hash = uint64(i->_materialId);
            auto insertPosition = std::lower_bound(materialIds.begin(), materialIds.end(), hash);
            if (insertPosition != materialIds.end() && *insertPosition == hash) continue;
            materialIds.insert(insertPosition, hash);
        }

        size_t finalIndexCount = 0;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
            finalDrawOperations.push_back(
                NascentDrawCallDesc(
                    (unsigned)finalIndexCount, (unsigned)i->_indexBuffer.size(), 0, 
                    (unsigned)std::distance(materialIds.begin(), std::lower_bound(materialIds.begin(), materialIds.end(), uint64(i->_materialId))),
                    i->_topology));
            finalIndexCount += i->_indexBuffer.size();
        }

            //  \todo -- sort by material id?

        Metal::NativeFormat::Enum indexFormat;
        std::unique_ptr<uint8[]> finalIndexBuffer;
        size_t finalIndexBufferSize;
                
        if (finalIndexCount < 0xffff) {

            size_t accumulatingIndexCount = 0;
            indexFormat = Metal::NativeFormat::R16_UINT;
            finalIndexBufferSize = finalIndexCount*sizeof(uint16);
            finalIndexBuffer = std::make_unique<uint8[]>(finalIndexBufferSize);
            for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
                size_t count = i->_indexBuffer.size();
                std::copy(
                    i->_indexBuffer.begin(), i->_indexBuffer.end(),
                    &((uint16*)finalIndexBuffer.get())[accumulatingIndexCount]);
                accumulatingIndexCount += count;
            }
            assert(accumulatingIndexCount==finalIndexCount);

        } else {

            size_t accumulatingIndexCount = 0;
            indexFormat = Metal::NativeFormat::R32_UINT;
            finalIndexBufferSize = finalIndexCount*sizeof(uint32);
            finalIndexBuffer = std::make_unique<uint8[]>(finalIndexBufferSize);
            for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
                size_t count = i->_indexBuffer.size();
                std::copy(
                    i->_indexBuffer.begin(), i->_indexBuffer.end(),
                    &((uint32*)finalIndexBuffer.get())[accumulatingIndexCount]);
                accumulatingIndexCount += count;
            }
            assert(accumulatingIndexCount==finalIndexCount);

        }

            //
            //      We've built everything:
            //          vertex buffer
            //          vertex input layout
            //          index buffer
            //          draw calls (and material ids)
            //
            //      Create the final RawGeometry object with all this stuff
            //

        return NascentRawGeometry(
            DynamicArray<uint8>(std::move(finalVertexBuffer), vertexSize*vertexCount), 
            DynamicArray<uint8>(std::move(finalIndexBuffer), finalIndexBufferSize),
            GeometryInputAssembly(std::move(nativeElements), (unsigned)vertexSize),
            indexFormat,
            std::move(finalDrawOperations),
            DynamicArray<uint32>(std::move(unifiedVertexIndexToPositionIndex), vertexCount),
            std::move(materialIds));
    }





    GeometryInputAssembly::GeometryInputAssembly(   std::vector<Metal::InputElementDesc>&& vertexInputLayout,
                                                    unsigned vertexStride)
    :       _vertexStride(vertexStride)
    ,       _vertexInputLayout(vertexInputLayout)
    {
    }

    GeometryInputAssembly::GeometryInputAssembly() 
    :   _vertexStride(0)
    {
    }

    void    GeometryInputAssembly::Serialize(Serialization::NascentBlockSerializer& outputSerializer, unsigned slotFilter) const
    {
        Serialization::NascentBlockSerializer subBlock;
        unsigned elementCount = 0;
        for (auto i=_vertexInputLayout.begin(); i!=_vertexInputLayout.end(); ++i) {
            if (slotFilter == ~unsigned(0x0) || i->_inputSlot == slotFilter) {
                char semantic[16];
                XlZeroMemory(semantic);     // make sure unused space is 0
                XlCopyNString(semantic, AsPointer(i->_semanticName.begin()), i->_semanticName.size());
                semantic[dimof(semantic)-1] = '\0';
                for (unsigned c=0; c<dimof(semantic); ++c) { subBlock.SerializeValue((uint8)semantic[c]); }
                subBlock.SerializeValue(i->_semanticIndex);
                subBlock.SerializeValue(unsigned(i->_nativeFormat));
                subBlock.SerializeValue(i->_alignedByteOffset);
                ++elementCount;
            }
        }
        outputSerializer.SerializeSubBlock(subBlock);
        outputSerializer.SerializeValue(elementCount);
        outputSerializer.SerializeValue(_vertexStride);
    }





    NascentRawGeometry::NascentRawGeometry(DynamicArray<uint8>&&    vb,
                                DynamicArray<uint8>&&               ib,
                                GeometryInputAssembly&&             mainDrawInputAssembly,
                                Metal::NativeFormat::Enum           indexFormat,
                                std::vector<NascentDrawCallDesc>&&  mainDrawCalls,
                                DynamicArray<uint32>&&              unifiedVertexIndexToPositionIndex,
                                std::vector<uint64>&&               materials)
    :       _vertices(std::forward<DynamicArray<uint8>>(vb))
    ,       _indices(std::forward<DynamicArray<uint8>>(ib))
    ,       _mainDrawCalls(std::forward<std::vector<NascentDrawCallDesc>>(mainDrawCalls))
    ,       _mainDrawInputAssembly(std::forward<GeometryInputAssembly>(mainDrawInputAssembly))
    ,       _indexFormat(indexFormat)
    ,       _unifiedVertexIndexToPositionIndex(std::forward<DynamicArray<uint32>>(unifiedVertexIndexToPositionIndex))
    ,       _materials(std::forward<std::vector<uint64>>(materials))
    {
    }

    NascentRawGeometry::NascentRawGeometry(NascentRawGeometry&& moveFrom)
    :       _vertices(std::move(moveFrom._vertices))
    ,       _indices(std::move(moveFrom._indices))
    ,       _mainDrawInputAssembly(std::move(moveFrom._mainDrawInputAssembly))
    ,       _indexFormat(moveFrom._indexFormat)
    ,       _mainDrawCalls(std::move(moveFrom._mainDrawCalls))
    ,       _unifiedVertexIndexToPositionIndex(std::move(moveFrom._unifiedVertexIndexToPositionIndex))
    ,       _materials(std::move(moveFrom._materials))
    {
    }

    NascentRawGeometry& NascentRawGeometry::operator=(NascentRawGeometry&& moveFrom)
    {
        _vertices = std::move(moveFrom._vertices);
        _indices = std::move(moveFrom._indices);
        _mainDrawInputAssembly = std::move(moveFrom._mainDrawInputAssembly);
        _indexFormat = moveFrom._indexFormat;
        _mainDrawCalls = std::move(moveFrom._mainDrawCalls);
        _unifiedVertexIndexToPositionIndex = std::move(moveFrom._unifiedVertexIndexToPositionIndex);
        _materials = std::move(moveFrom._materials);
        return *this;
    }

    NascentRawGeometry::NascentRawGeometry()
    : _vertices(nullptr, 0)
    , _indices(nullptr, 0)
    , _unifiedVertexIndexToPositionIndex(nullptr, 0)
    {
        _indexFormat = Metal::NativeFormat::Unknown;
    }

    void    NascentRawGeometry::Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const
    {
            //  We're going to write the index and vertex buffer data to the "large resources block"
            //  class members and scaffold structure get written to the serialiser, but the very large stuff
            //  should end up in a separate pool

        auto vbOffset = largeResourcesBlock.size();
        auto vbSize = _vertices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _vertices.begin(), _vertices.end());

        auto ibOffset = largeResourcesBlock.size();
        auto ibSize = _indices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _indices.begin(), _indices.end());

        _mainDrawInputAssembly.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset));
        outputSerializer.SerializeValue(unsigned(vbSize));
        outputSerializer.SerializeValue(unsigned(_indexFormat));
        outputSerializer.SerializeValue(unsigned(ibOffset));
        outputSerializer.SerializeValue(unsigned(ibSize));
        
        outputSerializer.SerializeSubBlock(AsPointer(_mainDrawCalls.begin()), AsPointer(_mainDrawCalls.end()));
        outputSerializer.SerializeValue(unsigned(_mainDrawCalls.size()));
    }




    void    NascentDrawCallDesc::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        outputSerializer.SerializeValue(_firstIndex);
        outputSerializer.SerializeValue(_indexCount);
        outputSerializer.SerializeValue(_firstVertex);
        outputSerializer.SerializeValue(_subMaterialIndex);
        outputSerializer.SerializeValue(unsigned(_topology));
    }

}}


