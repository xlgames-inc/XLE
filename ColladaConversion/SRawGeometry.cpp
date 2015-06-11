// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "Scaffold.h"
#include "RawGeometry.h"
#include "ProcessingUtil.h"
#include "ParsingUtil.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <map>
#include <set>

namespace ColladaConversion
{
    using namespace RenderCore;
    using namespace RenderCore::ColladaConversion;

    std::shared_ptr<std::vector<uint8>> GetParseDataSource();

    class VertexSourceData : public IVertexSourceData
    {
    public:
        const void* GetData() const;
        size_t GetDataSize() const;
        RenderCore::Metal::NativeFormat::Enum GetFormat() const { return _dataFormat; }
        size_t GetStride() const { return _stride; }
        size_t GetCount() const { return _count; }
        ProcessingFlags::BitField GetProcessingFlags() const { return 0; }
        FormatHint::BitField GetFormatHint() const { return _formatHint; }

        VertexSourceData(const DataFlow::Source& source);
        ~VertexSourceData();

    protected:
        RenderCore::Metal::NativeFormat::Enum _dataFormat;
        size_t _stride;
        size_t _offset;
        size_t _count;
        FormatHint::BitField _formatHint;

        std::shared_ptr<std::vector<uint8>> _rawData;
    };

    const void* VertexSourceData::GetData() const
    {
        return PtrAdd(AsPointer(_rawData->cbegin()), _offset);
    }

    size_t VertexSourceData::GetDataSize() const
    {
        return _rawData->size();
    }


    VertexSourceData::VertexSourceData(const DataFlow::Source& source)
    {
        auto accessor = source.FindAccessorForTechnique();
        if (!accessor)
            Throw(FormatException("Vertex data source missing accessor for technique_common", source.GetLocation()));

        auto sourceType = source.GetType();

            // we support only a small set of possible layouts
            // for parameters in the accessor. Validate what we
            // see, and throw an exception if we get something wierd
        auto paramCount = accessor->GetParamCount();
        if (paramCount < 1 || paramCount > 4)
            Throw(FormatException("Too many or too few parameters in vertex data source", source.GetLocation()));

        for (size_t c=0; c<paramCount; ++c)
            if (accessor->GetParam(c)._offset != c) {
                Throw(FormatException("Accessor params appear out-of-order, or not sequential", source.GetLocation()));
            } else if (accessor->GetParam(c)._type != sourceType) {
                Throw(FormatException("Accessor params type doesn't match source array type", source.GetLocation()));
            }

        using namespace Metal::NativeFormat;
        unsigned parsedTypeSize;
        Metal::NativeFormat::Enum finalFormat;
        if (sourceType == DataFlow::ArrayType::Int) {
            switch (paramCount) {
            case 1: finalFormat = R32_FLOAT; break;
            case 2: finalFormat = R32G32_FLOAT; break;
            case 3: finalFormat = R32G32B32_FLOAT; break;
            default: finalFormat = R32G32B32A32_FLOAT; break;
            }
            parsedTypeSize = sizeof(float);
        } else if (sourceType == DataFlow::ArrayType::Float) {
            switch (paramCount) {
            case 1: finalFormat = R32_UINT; break;
            case 2: finalFormat = R32G32_UINT; break;
            case 3: finalFormat = R32G32B32_UINT; break;
            default: finalFormat = R32G32B32A32_UINT; break;
            }
            parsedTypeSize = sizeof(unsigned);
        } else
            Throw(FormatException("Data source type is not supported for vertex data", source.GetLocation()));

        _offset = accessor->GetOffset() * parsedTypeSize;
        _stride = accessor->GetStride() * parsedTypeSize;
        _count = source.GetCount();
        _dataFormat = finalFormat;
        _formatHint = 0;

            // if the first param is called 'r', we assume it is a color
            // (only supporting R, G, B order)
        auto p0Name = accessor->GetParam(0)._name;
        if ((p0Name._end > p0Name._start) && tolower(*(const char*)p0Name._start) == 'r')
            _formatHint |= FormatHint::IsColor;

        _rawData = std::make_shared<std::vector<uint8>>(source.GetCount() * parsedTypeSize);
        if (sourceType == DataFlow::ArrayType::Int) {
            ParseXMLList((uint32*)AsPointer(_rawData->begin()), (unsigned)source.GetCount(), source.GetArrayData());
        } else if (sourceType == DataFlow::ArrayType::Float) {
            ParseXMLList((float*)AsPointer(_rawData->begin()), (unsigned)source.GetCount(), source.GetArrayData());
        }
    }

    VertexSourceData::~VertexSourceData() {}

    
    class ComposingVertex
    {
    public:
        class Element
        {
        public:
            Section _semantic;
            unsigned _semanticIndex;
            std::shared_ptr<IVertexSourceData> _sourceData;
            uint64 _sourceId;
        };
        std::vector<Element> _finalVertexElements;

        size_t FindOrCreateElement(const DataFlow::Source& source, Section semantic, unsigned semanticIndex)
        {
                // We need to find something matching this in _finalVertexElements
            size_t existing = ~size_t(0x0);
            for (size_t c=0; c<_finalVertexElements.size(); ++c)
                if (    _finalVertexElements[c]._sourceId == source.GetId().GetHash()
                    &&  Equivalent(_finalVertexElements[c]._semantic, semantic)
                    &&  _finalVertexElements[c]._semanticIndex == semanticIndex)
                    existing = c;

            if (existing == ~size_t(0x0)) {
                Element newEle;
                newEle._sourceId = source.GetId().GetHash();
                newEle._semantic = semantic;
                newEle._semanticIndex = semanticIndex;
                newEle._sourceData = std::make_shared<VertexSourceData>(source);
                _finalVertexElements.push_back(newEle);
                existing = _finalVertexElements.size()-1;
            }

            return existing;
        }
    };

    #define UNIFIED_VERTS_USE_MAP

    class ComposingUnifiedVertices
    {
    public:
        std::vector<size_t>     _unifiedToAttributeIndex;
        size_t                  _attributesPerVertex;

            // We get a very high number of inserts per queries with
            // this data structure. And it grows to a very last size.
            // So, this is a case where a std::map is likely to be best.
        #if defined(UNIFIED_VERTS_USE_MAP)
            std::map<uint64, size_t> _hashValues;
        #else
            std::vector<std::pair<uint64, size_t>>    _hashValues;
        #endif
        
        size_t BuildUnifiedVertex(const size_t attributeIndices[]);

        ComposingUnifiedVertices(size_t attributesPerVertex);
        ~ComposingUnifiedVertices();

        ComposingUnifiedVertices(const ComposingUnifiedVertices&) = delete;
        ComposingUnifiedVertices& operator=(const ComposingUnifiedVertices&) = delete;
    };

    size_t ComposingUnifiedVertices::BuildUnifiedVertex(const size_t attributeIndices[])
    {
        uint64 hashValue = 0;
        for (unsigned c=0; c<_attributesPerVertex; ++c)
            hashValue = HashCombine(attributeIndices[c], hashValue);

        #if defined(UNIFIED_VERTS_USE_MAP)
            auto i = _hashValues.lower_bound(hashValue);
        #else
            auto i = LowerBound(_hashValues, hashValue);
        #endif

        if (i != _hashValues.cend() && i->first == hashValue) {
            #if defined(_DEBUG)
                auto* val = &_unifiedToAttributeIndex[i->second*_attributesPerVertex];
                for (unsigned c=0; c<_attributesPerVertex; ++c)
                    assert(val[c] == attributeIndices[c]);
            #endif
            return i->second;
        }

        auto newIndex = _unifiedToAttributeIndex.size() / _attributesPerVertex;
        _unifiedToAttributeIndex.insert(_unifiedToAttributeIndex.end(), attributeIndices, &attributeIndices[_attributesPerVertex]);

        _hashValues.insert(i, std::make_pair(hashValue, newIndex));
        return newIndex;
    }

    ComposingUnifiedVertices::ComposingUnifiedVertices(size_t attributesPerVertex) : _attributesPerVertex(attributesPerVertex) {}
    ComposingUnifiedVertices::~ComposingUnifiedVertices() {}

    std::shared_ptr<MeshDatabaseAdapter> BuildMeshDatabaseAdapter(
        ComposingVertex& composingVert,
        ComposingUnifiedVertices& unifiedVerts)
    {
        auto result = std::make_shared<MeshDatabaseAdapter>();

        auto attribCount = composingVert._finalVertexElements.size();
        if (!attribCount || attribCount != unifiedVerts._attributesPerVertex) return nullptr;

        auto unifiedVertCount = unifiedVerts._unifiedToAttributeIndex.size() / attribCount;

        for (auto i=composingVert._finalVertexElements.cbegin(); i!=composingVert._finalVertexElements.cend(); ++i) {
            auto index = std::distance(composingVert._finalVertexElements.cbegin(), i);

            std::vector<unsigned> vertexMap(unifiedVertCount);
            for (unsigned v=0; v<unifiedVertCount; ++v)
                vertexMap[v] = (unsigned)unifiedVerts._unifiedToAttributeIndex[v*attribCount + index];

            result->AddStream(
                i->_sourceData, std::move(vertexMap),
                AsString(i->_semantic).c_str(), i->_semanticIndex);
        }

        return std::move(result);
    }

    enum class PrimitiveTopology
    {
        Unknown, Triangles, TriangleStrips
    };

    std::pair<PrimitiveTopology, const utf8*> s_PrimitiveTopologyNames[] = 
    {
        std::make_pair(PrimitiveTopology::Unknown, u("unknown")),
        std::make_pair(PrimitiveTopology::Triangles, u("triangles")),
        std::make_pair(PrimitiveTopology::TriangleStrips, u("tristrips"))
    };

    static PrimitiveTopology AsPrimitiveTopology(Section section)
    {
        return ParseEnum(section, s_PrimitiveTopologyNames);
    }

    class WorkingDrawOperation
    {
    public:
        std::vector<unsigned>   _indexBuffer;
        Metal::Topology::Enum   _topology;
        uint64                  _materialBinding;

        WorkingDrawOperation() : _topology((Metal::Topology::Enum)0), _materialBinding(0) {}
        WorkingDrawOperation(WorkingDrawOperation&& moveFrom) never_throws
        : _indexBuffer(std::move(moveFrom._indexBuffer))
        , _topology(moveFrom._topology)
        , _materialBinding(moveFrom._materialBinding)
        {}

        WorkingDrawOperation& operator=(WorkingDrawOperation&& moveFrom) never_throws
        {
            _indexBuffer = std::move(moveFrom._indexBuffer);
            _topology = moveFrom._topology;
            _materialBinding = moveFrom._materialBinding;
            return *this;
        }

        WorkingDrawOperation(const WorkingDrawOperation&) = delete;
        WorkingDrawOperation& operator=(const WorkingDrawOperation&) = delete;
    };

    NascentRawGeometry Convert(const MeshGeometry& mesh, const URIResolveContext& pubEles)
    {
            // some exports can have empty meshes -- ideally, we just want to ignore them
        if (!mesh.GetPrimitivesCount()) return NascentRawGeometry();

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

        std::vector<WorkingDrawOperation>  drawOperations;
        ComposingVertex composingVertex;

        class WorkingPrimitive
        {
        public:
            class EleRef
            {
            public:
                size_t _mappedInput;
                unsigned _indexInPrimitive;
            };
            std::vector<EleRef> _inputs;
            unsigned _primitiveStride;

            std::vector<unsigned> _unifiedVertices;
        };

        std::vector<WorkingPrimitive> workingPrims;

            //
///////////////////////////////////////////////////////////////////////////////////////////////////
            //      First, decide on the layout of the vertices
            //
            //      This is a bit awkward. Each primitive has it's own list
            //      of vertex inputs. We need to map those onto a global set
            //      of elements that are shared by all vertices in the the mesh.
            //
            //      We make an assumption here that there is only one data source
            //      for each semantic. Collada allows each primitive to specify a
            //      different data source for each primitive. But we will require
            //      that there must be no more than one for each (eg, one data 
            //      source for positions, one for normals, etc)
            //

        for (size_t c=0; c<mesh.GetPrimitivesCount(); ++c) {
            const auto& geoPrim = mesh.GetPrimitives(c);
            
            WorkingPrimitive workingPrim;
            workingPrim._primitiveStride = 0;

            auto inputCount = geoPrim.GetInputCount();
            for (size_t c=0; c<inputCount; ++c) {
                const auto& input = geoPrim.GetInput(c);

                    // the "source" can be either a DataFlow::Source, or
                    // it can be a VertexInputs

                GuidReference ref(input._source);
                const auto* file = pubEles.FindFile(ref._fileHash);
                if (!file) continue;

                const auto* source = file->FindSource(ref._id);
                if (source) {

                    workingPrim._inputs.push_back(
                        WorkingPrimitive::EleRef
                        {
                            composingVertex.FindOrCreateElement(*source, input._semantic, input._semanticIndex),
                            input._indexInPrimitive
                        });

                } else {

                    const auto* extraInputs = file->FindVertexInputs(ref._id);
                    if (extraInputs) {
                        for (size_t c=0; c<extraInputs->GetCount(); ++c) {
                            const auto& refInput = extraInputs->GetInput(c);

                                // find the true source by looking up the reference
                                // provided within the <vertex> elemenet
                            const DataFlow::Source* source = nullptr;
                            GuidReference secondRef(refInput._source);
                            const auto* file = pubEles.FindFile(secondRef._fileHash);
                            if (file) source = file->FindSource(secondRef._id);
                            if (!source) continue;

                                // push back with the semantic name from the <vertex> element
                                // but the semantic index from the <input> element
                            workingPrim._inputs.push_back(
                                WorkingPrimitive::EleRef
                                {
                                    composingVertex.FindOrCreateElement(*source, refInput._semantic, input._semanticIndex),
                                    input._indexInPrimitive
                                });
                        }
                    }

                }
            }

            for (const auto& i:workingPrim._inputs)
                workingPrim._primitiveStride = std::max(i._indexInPrimitive+1, workingPrim._primitiveStride);

            workingPrims.push_back(workingPrim);
        }

            //
///////////////////////////////////////////////////////////////////////////////////////////////////
            //

        std::vector<size_t> vertexTemp(composingVertex._finalVertexElements.size());
        ComposingUnifiedVertices composingUnified(composingVertex._finalVertexElements.size());

        for (size_t c=0; c<mesh.GetPrimitivesCount(); ++c) {
            const auto& geoPrim = mesh.GetPrimitives(c);
            const auto& workingPrim = workingPrims[c];

            auto type = AsPrimitiveTopology(geoPrim.GetType());
            if (type == PrimitiveTopology::Triangles) {

                if (geoPrim.GetPrimitiveDataCount() != 1)
                    Throw(FormatException("Expecting only a single <p> element", geoPrim.GetLocation()));

                    // Simpliest arrangement. We should have a single <p> element with
                    // just a single of indices. Parse in those value, and we will use
                    // them to generate unified vertices.

                auto indexCount = geoPrim.GetPrimitiveCount() * 3;
                auto valueCount = indexCount * workingPrim._primitiveStride;
                auto rawIndices = std::make_unique<unsigned[]>(valueCount);
                ParseXMLList(AsPointer(rawIndices.get()), valueCount, geoPrim.GetPrimitiveData(0));

                std::vector<unsigned> finalIndices(indexCount);
                for (size_t i=0; i<indexCount; ++i) {
                        // note that when an element is not specified, it ends up with zero
                    std::fill(vertexTemp.begin(), vertexTemp.end(), 0);
                    const auto* rawI = &rawIndices[i*workingPrim._primitiveStride];

                    for (const auto& e:workingPrim._inputs)
                        vertexTemp[e._mappedInput] = rawI[e._indexInPrimitive];

                    finalIndices[i] = (unsigned)composingUnified.BuildUnifiedVertex(AsPointer(vertexTemp.begin()));
                }

                WorkingDrawOperation drawCall;
                drawCall._indexBuffer = std::move(finalIndices);
                drawCall._materialBinding = Hash64(geoPrim.GetMaterialBinding()._start, geoPrim.GetMaterialBinding()._end);
                drawCall._topology = RenderCore::Metal::Topology::TriangleList;
                drawOperations.push_back(std::move(drawCall));

            } else 
                Throw(FormatException("Unsupported primitive type found", geoPrim.GetLocation()));
        }

            //
///////////////////////////////////////////////////////////////////////////////////////////////////
            //

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

        auto database = BuildMeshDatabaseAdapter(composingVertex, composingUnified);

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

        std::set<uint64> matBindingSymbols;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i)
            matBindingSymbols.insert(i->_materialBinding);

        size_t finalIndexCount = 0;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
            assert(i->_topology == Metal::Topology::TriangleList);  // tangent generation assumes triangle list currently... We need to modify GenerateNormalsAndTangents() to support other types
            finalDrawOperations.push_back(
                NascentDrawCallDesc(
                    (unsigned)finalIndexCount, (unsigned)i->_indexBuffer.size(), 0, 
                    (unsigned)std::distance(matBindingSymbols.begin(), matBindingSymbols.find(i->_materialBinding)),
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

            //  Once we have the index buffer, we can generate tangent vectors (if we need to)
            //  We need the triangulation in order to build the tangents, so it must be done
            //  after the index buffer is finalized
        const bool generateMissingTangentsAndNormals = true;
        if (constant_expression<generateMissingTangentsAndNormals>::result()) {
            GenerateNormalsAndTangents(
                *database, 0,
                finalIndexBuffer.get(), finalIndexCount, indexFormat);
        }

        NativeVBLayout vbLayout = BuildDefaultLayout(*database);
        auto nativeVB = database->BuildNativeVertexBuffer(vbLayout);

            // note -- this should actually be the mapping onto the input with the semantic "VERTEX" in the primitives' input array
        auto unifiedVertexIndexToPositionIndex = database->BuildUnifiedVertexIndexToPositionIndex();

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
            DynamicArray<uint8>(std::move(nativeVB), vbLayout._vertexStride * database->_unifiedVertexCount), 
            DynamicArray<uint8>(std::move(finalIndexBuffer), finalIndexBufferSize),
            GeometryInputAssembly(std::move(vbLayout._elements), (unsigned)vbLayout._vertexStride),
            indexFormat,
            std::move(finalDrawOperations),
            DynamicArray<uint32>(std::move(unifiedVertexIndexToPositionIndex), database->_unifiedVertexCount),
            std::vector<uint64>(matBindingSymbols.cbegin(), matBindingSymbols.cend()));
    }

}

