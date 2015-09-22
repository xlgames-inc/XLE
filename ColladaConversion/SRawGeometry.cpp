// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "SRawGeometry.h"
#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"
#include "NascentRawGeometry.h"
#include "NascentAnimController.h"
#include "GeometryAlgorithm.h"
#include "ConversionUtil.h"
#include "../RenderCore/Assets/MeshDatabase.h"
#include "../RenderCore/Metal/DeviceContext.h"      // for Topology...!
#include "../ConsoleRig/Log.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <map>
#include <set>

namespace ColladaConversion
{
    using namespace RenderCore;
    using namespace RenderCore::ColladaConversion;
    using namespace RenderCore::Assets::GeoProc;

    static const std::string DefaultSemantic_Weights         = "WEIGHTS";
    static const std::string DefaultSemantic_JointIndices    = "JOINTINDICES";

    static const unsigned AbsoluteMaxJointInfluenceCount = 256;

    const NativeVBSettings NativeSettings = { true };       // use 16 bit floats

    std::shared_ptr<std::vector<uint8>> GetParseDataSource();

    class VertexSourceData : public IVertexSourceData
    {
    public:
        const void* GetData() const;
        size_t GetDataSize() const;
        RenderCore::Metal::NativeFormat::Enum GetFormat() const { return _dataFormat; }
        size_t GetStride() const { return _stride; }
        size_t GetCount() const { return _count; }
        ProcessingFlags::BitField GetProcessingFlags() const { return _processingFlags; }
        FormatHint::BitField GetFormatHint() const { return _formatHint; }

        VertexSourceData(const DataFlow::Source& source, ProcessingFlags::BitField processingFlags);
        ~VertexSourceData();

    protected:
        RenderCore::Metal::NativeFormat::Enum _dataFormat;
        size_t _stride;
        size_t _offset;
        size_t _count;
        FormatHint::BitField _formatHint;
        ProcessingFlags::BitField _processingFlags;

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


    VertexSourceData::VertexSourceData(const DataFlow::Source& source, ProcessingFlags::BitField processingFlags)
        : _processingFlags(processingFlags)
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
            } /*else if (accessor->GetParam(c)._type != sourceType) {
                Throw(FormatException("Accessor params type doesn't match source array type", source.GetLocation()));
            }*/

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
        _count = accessor->GetCount();
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

    static ProcessingFlags::BitField GetProcessingFlags(const std::basic_string<utf8>& semantic)
    {
        const auto npos = std::basic_string<utf8>::npos;
        if (semantic.find(u("TEXCOORD")) != npos || semantic.find(u("texcoord")) != npos) {
            return ProcessingFlags::TexCoordFlip;
        } else if (semantic.find(u("TEXTANGENT")) != npos || semantic.find(u("textangent")) != npos) {
            return ProcessingFlags::Renormalize;
        } else if (semantic.find(u("TEXBITANGENT")) != npos || semantic.find(u("texbitangent")) != npos) {
            return ProcessingFlags::Renormalize;
        } /*else if (semantic.find(u("NORMAL")) != npos || semantic.find(u("normal")) != npos) {
            return ProcessingFlags::Renormalize;
        }*/
        return 0;
    }
    
    class ComposingVertex
    {
    public:
        class Element
        {
        public:
            std::basic_string<utf8> _semantic;
            unsigned _semanticIndex;
            std::shared_ptr<IVertexSourceData> _sourceData;
            uint64 _sourceId;
        };
        std::vector<Element> _finalVertexElements;
        const ImportConfiguration* _cfg;

        size_t FindOrCreateElement(const DataFlow::Source& source, Section semantic, unsigned semanticIndex)
        {
            std::basic_string<utf8> semanticStr;
            if (_cfg) {
                if (_cfg->GetVertexSemanticBindings().IsSuppressed(semantic._start, semantic._end))
                    return ~size_t(0);
                semanticStr = _cfg->GetVertexSemanticBindings().AsNative(semantic._start, semantic._end);
            } else {
                semanticStr = std::basic_string<utf8>(semantic._start, semantic._end);
            }

                // We need to find something matching this in _finalVertexElements
            size_t existing = ~size_t(0x0);
            for (size_t c=0; c<_finalVertexElements.size(); ++c)
                if (    _finalVertexElements[c]._sourceId == source.GetId().GetHash()
                    &&  _finalVertexElements[c]._semantic == semanticStr
                    &&  _finalVertexElements[c]._semanticIndex == semanticIndex)
                    existing = c;

            if (existing == ~size_t(0x0)) {
                Element newEle;
                newEle._sourceId = source.GetId().GetHash();
                newEle._semantic = semanticStr;
                newEle._semanticIndex = semanticIndex;
                newEle._sourceData = std::make_shared<VertexSourceData>(source, GetProcessingFlags(semanticStr));
                _finalVertexElements.push_back(newEle);
                existing = _finalVertexElements.size()-1;
            }

            return existing;
        }

        void FixBadSemanticIndicies()
        {
            // There seems to be a problem in the max exporter whereby some vertex inputs
            // are assigned to the incorrect semantic index. They appear to be starting from
            // "1" (instead of "0"). DirectX defaults to starting from 0 -- so we have to 
            // shift the indices down to adjust for this!

            std::set<std::basic_string<utf8>> semanticNames;
            for (const auto&i:_finalVertexElements)
                semanticNames.insert(i._semantic);

            for (const auto&s:semanticNames) {
                unsigned minIndex = ~0u;
                for (size_t c=0; c<_finalVertexElements.size(); ++c)
                    if (_finalVertexElements[c]._semantic == s)
                        minIndex = std::min(minIndex, _finalVertexElements[c]._semanticIndex);

                for (size_t c=0; c<_finalVertexElements.size(); ++c)
                    if (_finalVertexElements[c]._semantic == s)
                        _finalVertexElements[c]._semanticIndex -= minIndex;
            }
        }

        ComposingVertex() : _cfg(nullptr) {}
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

    std::shared_ptr<MeshDatabase> BuildMeshDatabaseAdapter(
        ComposingVertex& composingVert,
        ComposingUnifiedVertices& unifiedVerts)
    {
        auto result = std::make_shared<MeshDatabase>();

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
                Conversion::Convert<std::string>(i->_semantic).c_str(), i->_semanticIndex);
        }

        return std::move(result);
    }

    enum class PrimitiveTopology { Unknown, Triangles, TriangleStrips, PolyList, Polygons };

    std::pair<PrimitiveTopology, const utf8*> s_PrimitiveTopologyNames[] = 
    {
        std::make_pair(PrimitiveTopology::Unknown, u("unknown")),
        std::make_pair(PrimitiveTopology::Triangles, u("triangles")),
        std::make_pair(PrimitiveTopology::TriangleStrips, u("tristrips")),
        std::make_pair(PrimitiveTopology::PolyList, u("polylist")),
        std::make_pair(PrimitiveTopology::Polygons, u("polygons"))
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

    static WorkingDrawOperation LoadTriangles(
        const GeometryPrimitives& geoPrim,
        const WorkingPrimitive& workingPrim,
        std::vector<size_t>& vertexTemp,
        ComposingUnifiedVertices& composingUnified)
    {
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
            const auto* rawI = &rawIndices[i*workingPrim._primitiveStride];
            for (const auto& e:workingPrim._inputs)
                vertexTemp[e._mappedInput] = rawI[e._indexInPrimitive];

            finalIndices[i] = (unsigned)composingUnified.BuildUnifiedVertex(AsPointer(vertexTemp.begin()));
        }

        WorkingDrawOperation drawCall;
        drawCall._indexBuffer = std::move(finalIndices);
        drawCall._materialBinding = Hash64(geoPrim.GetMaterialBinding()._start, geoPrim.GetMaterialBinding()._end);
        drawCall._topology = RenderCore::Metal::Topology::TriangleList;
        return std::move(drawCall);
    }

    static WorkingDrawOperation LoadPolyList(
        const GeometryPrimitives& geoPrim,
        const WorkingPrimitive& workingPrim,
        std::vector<size_t>& vertexTemp,
        ComposingUnifiedVertices& composingUnified)
    {
            // We should have a single <vcount> element that contains the number of vertices
            // in each polygon. There should also be a single <p> element with the 
            // actual vertex indices.
            // Note that this is very similar to <polygons> (polygons requires one <p>
            // element per polygon, and also supports holes in polygons)

        if (geoPrim.GetPrimitiveDataCount() != 1)
            Throw(FormatException("Expecting only a single <p> element", geoPrim.GetLocation()));

        auto vcountSrc = geoPrim.GetVCountArray();
        if (!(vcountSrc._end > vcountSrc._start))
            Throw(FormatException("Expecting a single <vcount> element", geoPrim.GetLocation()));

        std::vector<unsigned> vcount(geoPrim.GetPrimitiveCount());
        ParseXMLList(AsPointer(vcount.begin()), (unsigned)vcount.size(), vcountSrc);
        std::vector<unsigned> finalIndices;
        finalIndices.reserve(vcount.size()*6);
                
        auto pIterator = geoPrim.GetPrimitiveData(0);

            // we're going to convert each polygon into triangles using
            // primitive triangulation...

        std::vector<unsigned> rawIndices(32 * workingPrim._primitiveStride);
        std::vector<unsigned> unifiedVertexIndices(32);
        std::vector<unsigned> windingRemap(32*2);
        for (auto v : vcount) {
            auto indiciesToLoad = v * workingPrim._primitiveStride;

            if (rawIndices.size() < indiciesToLoad) rawIndices.resize(indiciesToLoad);
            if (windingRemap.size() < (v*3))        windingRemap.resize(v*3);
            if (unifiedVertexIndices.size() < v)    unifiedVertexIndices.resize(v);

            pIterator._start = ParseXMLList(AsPointer(rawIndices.begin()), indiciesToLoad, pIterator);

                // build "unified" vertices from the list of vertices provided here
            for (auto q=0u; q<v; ++q) {
                const auto* rawI = &rawIndices[q*workingPrim._primitiveStride];
                for (const auto& e:workingPrim._inputs)
                    vertexTemp[e._mappedInput] = rawI[e._indexInPrimitive];

                unifiedVertexIndices[q] = (unsigned)composingUnified.BuildUnifiedVertex(AsPointer(vertexTemp.begin()));
            }

            size_t triangleCount = CreateTriangleWindingFromPolygon(
                AsPointer(windingRemap.begin()), windingRemap.size(), v);
            assert((triangleCount*3) <= windingRemap.size());

                // Remap from arbitrary polygon order into triange list order
                // note that we aren't careful about how we divide a polygon
                // up into triangles! If the polygon vertices are not coplanear
                // then the way we triangulate it will affect the final shape.
                // Also, if the polygon is not convex, strange things could happen.
            for (auto q=0u; q<triangleCount*3; ++q) {
                assert(windingRemap[q] < v);
                finalIndices.push_back((unsigned)unifiedVertexIndices[windingRemap[q]]);
            }
        }

        WorkingDrawOperation drawCall;
        drawCall._indexBuffer = std::move(finalIndices);
        drawCall._materialBinding = Hash64(geoPrim.GetMaterialBinding()._start, geoPrim.GetMaterialBinding()._end);
        drawCall._topology = RenderCore::Metal::Topology::TriangleList;
        return std::move(drawCall);
    }

    static WorkingDrawOperation LoadPolygons(
        const GeometryPrimitives& geoPrim,
        const WorkingPrimitive& workingPrim,
        std::vector<size_t>& vertexTemp,
        ComposingUnifiedVertices& composingUnified)
    {
            // We should have many <p> elements, each containing a single
            // (possibly non-convex, non-coplanear) polygon.
            // There can also be <ph> element for holes -- but we will ignore them.

        std::vector<unsigned> finalIndices;
                
        auto pIterator = geoPrim.GetPrimitiveData(0);

            // we're going to convert each polygon into triangles using
            // primitive triangulation...

        std::vector<unsigned> rawIndices(32 * workingPrim._primitiveStride);
        std::vector<unsigned> unifiedVertexIndices(32);
        std::vector<unsigned> windingRemap(32*2);
        for (unsigned p=0; p<geoPrim.GetPrimitiveCount(); ++p) {

            unsigned polygonIndices = 0;
            for (;;) {
                auto section = geoPrim.GetPrimitiveData(p);
                ParseXMLList(AsPointer(rawIndices.begin()), (unsigned)rawIndices.size(), section, &polygonIndices);
                
                // if we didn't parse the entire list, we need to increase our buffer and try again
                if (polygonIndices > rawIndices.size()) {
                    rawIndices.resize(polygonIndices);
                    continue;
                }

                // otherwise, we can break out of the loop
                break;
            }

            assert((polygonIndices % workingPrim._primitiveStride) == 0);
            auto polyVerts = polygonIndices / workingPrim._primitiveStride;
            
            if (windingRemap.size() < (polyVerts*3))        windingRemap.resize(polyVerts*3);
            if (unifiedVertexIndices.size() < polyVerts)    unifiedVertexIndices.resize(polyVerts);

                // build "unified" vertices from the list of vertices provided here
            for (auto q=0u; q<polyVerts; ++q) {
                const auto* rawI = &rawIndices[q*workingPrim._primitiveStride];
                for (const auto& e:workingPrim._inputs)
                    vertexTemp[e._mappedInput] = rawI[e._indexInPrimitive];

                unifiedVertexIndices[q] = (unsigned)composingUnified.BuildUnifiedVertex(AsPointer(vertexTemp.begin()));
            }

            size_t triangleCount = CreateTriangleWindingFromPolygon(
                AsPointer(windingRemap.begin()), windingRemap.size(), polyVerts);
            assert((triangleCount*3) <= windingRemap.size());

            for (auto q=0u; q<triangleCount*3; ++q) {
                assert(windingRemap[q] < polyVerts);
                finalIndices.push_back((unsigned)unifiedVertexIndices[windingRemap[q]]);
            }
        }

        WorkingDrawOperation drawCall;
        drawCall._indexBuffer = std::move(finalIndices);
        drawCall._materialBinding = Hash64(geoPrim.GetMaterialBinding()._start, geoPrim.GetMaterialBinding()._end);
        drawCall._topology = RenderCore::Metal::Topology::TriangleList;
        return std::move(drawCall);
    }

    NascentRawGeometry Convert(
        const MeshGeometry& mesh, 
        const URIResolveContext& pubEles, 
        const ImportConfiguration& cfg)
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
        composingVertex._cfg = &cfg;

        std::vector<WorkingPrimitive> workingPrims;
        bool atLeastOneInput = true;

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

                bool boundSource = false;
                    
                GuidReference ref(input._source);
                const auto* file = pubEles.FindFile(ref._fileHash);
                if (file) {

                    const auto* source = file->FindSource(ref._id);
                    if (source) {

                        workingPrim._inputs.push_back(
                            WorkingPrimitive::EleRef
                            {
                                composingVertex.FindOrCreateElement(*source, input._semantic, input._semanticIndex),
                                input._indexInPrimitive
                            });
                        boundSource = true;

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
                                if (source) {
                                        // push back with the semantic name from the <vertex> element
                                        // but the semantic index from the <input> element
                                    workingPrim._inputs.push_back(
                                        WorkingPrimitive::EleRef
                                        {
                                            composingVertex.FindOrCreateElement(
                                                *source, refInput._semantic, input._semanticIndex),
                                            input._indexInPrimitive
                                        });
                                    boundSource = true;
                                }
                            }
                        }

                    }
                }

                if (!boundSource) {
                    LogWarning << "Couldn't find source for geometry input. Source name: " << input._source << " in geometry " << mesh.GetName();
                }

                    // we must adjust the primitive stride, even if we couldn't properly resolve
                    // the input source
                workingPrim._primitiveStride = std::max(input._indexInPrimitive+1, workingPrim._primitiveStride);
            }

            atLeastOneInput |= workingPrim._inputs.size() > 0;
            workingPrims.push_back(workingPrim);
        }

        if (!atLeastOneInput) {
            LogWarning << "Geometry object with no valid vertex inputs: " << mesh.GetName();
            return NascentRawGeometry();
        }

        composingVertex.FixBadSemanticIndicies();

            //
///////////////////////////////////////////////////////////////////////////////////////////////////
            //

        std::vector<size_t> vertexTemp(composingVertex._finalVertexElements.size(), 0u);
        ComposingUnifiedVertices composingUnified(composingVertex._finalVertexElements.size());

        for (size_t c=0; c<mesh.GetPrimitivesCount(); ++c) {
            const auto& geoPrim = mesh.GetPrimitives(c);
            const auto& workingPrim = workingPrims[c];

            auto type = AsPrimitiveTopology(geoPrim.GetType());
            if (type == PrimitiveTopology::Triangles) {
                drawOperations.push_back(LoadTriangles(geoPrim, workingPrim, vertexTemp, composingUnified));
            } else if (type == PrimitiveTopology::PolyList) {
                drawOperations.push_back(LoadPolyList(geoPrim, workingPrim, vertexTemp, composingUnified));
            } else if (type == PrimitiveTopology::Polygons) {
                drawOperations.push_back(LoadPolygons(geoPrim, workingPrim, vertexTemp, composingUnified));
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

        std::vector<DrawCallDesc> finalDrawOperations;
        finalDrawOperations.reserve(drawOperations.size());

        std::set<uint64> matBindingSymbols;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i)
            matBindingSymbols.insert(i->_materialBinding);

        size_t finalIndexCount = 0;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
            assert(i->_topology == Metal::Topology::TriangleList);  // tangent generation assumes triangle list currently... We need to modify GenerateNormalsAndTangents() to support other types
            finalDrawOperations.push_back(
                DrawCallDesc(
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

        NativeVBLayout vbLayout = BuildDefaultLayout(*database, NativeSettings);
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
            DynamicArray<uint8>(std::move(nativeVB), vbLayout._vertexStride * database->GetUnifiedVertexCount()), 
            DynamicArray<uint8>(std::move(finalIndexBuffer), finalIndexBufferSize),
            CreateGeoInputAssembly(vbLayout._elements, (unsigned)vbLayout._vertexStride),
            indexFormat,
            std::move(finalDrawOperations),
            DynamicArray<uint32>(std::move(unifiedVertexIndexToPositionIndex), database->GetUnifiedVertexCount()),
            std::vector<uint64>(matBindingSymbols.cbegin(), matBindingSymbols.cend()));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    DynamicArray<Float4x4> GetInverseBindMatrices(
        const SkinController& skinController,
        const URIResolveContext& resolveContext)
    {
        auto invBindInput = skinController.GetJointInputs().FindInputBySemantic(u("INV_BIND_MATRIX"));
        if (!invBindInput) return DynamicArray<Float4x4>();

        auto* invBindSource = FindElement(
            GuidReference(invBindInput->_source), resolveContext, 
            &IDocScopeIdResolver::FindSource);
        if (!invBindSource) return DynamicArray<Float4x4>();

        if (invBindSource->GetType() != DataFlow::ArrayType::Float)
            return DynamicArray<Float4x4>();

        auto* commonAccessor = invBindSource->FindAccessorForTechnique();
        if (!commonAccessor) return DynamicArray<Float4x4>();

        if (    commonAccessor->GetParamCount() != 1
            || !Is(commonAccessor->GetParam(0)._type, u("float4x4"))) {
            LogWarning << "Expecting inverse bind matrices expressed as float4x4 elements. These inverse bind elements will be ignored. In <source> at " << invBindSource->GetLocation();
            return DynamicArray<Float4x4>();
        }

        auto count = commonAccessor->GetCount();
        if (!count)
            return DynamicArray<Float4x4>(nullptr, 0);

        DynamicArray<Float4x4> result(std::make_unique<Float4x4[]>(count), count);

            // parse in the array of raw floats
        auto rawFloatCount = invBindSource->GetCount();
        auto rawFloats = std::make_unique<float[]>(rawFloatCount);
        ParseXMLList(rawFloats.get(), (unsigned)rawFloatCount, invBindSource->GetArrayData());

        for (unsigned c=0; c<count; ++c) {
            auto r = c * commonAccessor->GetStride();
            if ((r + 16) <= rawFloatCount) {
                result[c] = MakeFloat4x4(
                    rawFloats[r+ 0], rawFloats[r+ 1], rawFloats[r+ 2], rawFloats[r+ 3],
                    rawFloats[r+ 4], rawFloats[r+ 5], rawFloats[r+ 6], rawFloats[r+ 7],
                    rawFloats[r+ 8], rawFloats[r+ 9], rawFloats[r+10], rawFloats[r+11],
                    rawFloats[r+12], rawFloats[r+13], rawFloats[r+14], rawFloats[r+15]);
            } else
                result[c] = Identity<Float4x4>();
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::vector<std::basic_string<utf8>> GetJointNames(
        const SkinController& controller,
        const URIResolveContext& resolveContext)
    {
        std::vector<std::basic_string<utf8>> result;

            // we're expecting an input called "JOINT" that contains the names of joints
            // These names should match the "sid" of nodes within the hierachy of nodes
            // beneath our "skeleton"
        auto jointInput = controller.GetJointInputs().FindInputBySemantic(u("JOINT"));
        if (!jointInput) return std::vector<std::basic_string<utf8>>();

        auto* jointSource = FindElement(
            GuidReference(jointInput->_source), resolveContext, 
            &IDocScopeIdResolver::FindSource);
        if (!jointSource) return std::vector<std::basic_string<utf8>>();

        if (    jointSource->GetType() != DataFlow::ArrayType::Name
            &&  jointSource->GetType() != DataFlow::ArrayType::SidRef)
            return std::vector<std::basic_string<utf8>>();

        auto count = jointSource->GetCount();
        auto arrayData = jointSource->GetArrayData();

            // data is stored in xml list format, with whitespace deliminated elements
            // there should be an <accessor> that describes how to read this list
            // But we're going to assume it just contains a single entry like:
            //      <param name="JOINT" type="name"/>
        result.reserve(count);
        unsigned elementIndex = 0;
        auto* i = arrayData._start;
        for (;;) {
            while (i < arrayData._end && IsWhitespace(*i)) ++i;
            if (i == arrayData._end) break;

            auto* elementStart = i;
            while (i < arrayData._end && !IsWhitespace(*i)) ++i;

            result.push_back(std::basic_string<utf8>(elementStart, i));
            if (i == arrayData._end || elementIndex == count) break;
        }

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type>
        DynamicArray<Type> AsScalarArray(const DataFlow::Source& source, const utf8 paramName[])
    {
        auto* accessor = source.FindAccessorForTechnique();
        if (!accessor) {
            LogWarning << "Expecting common access in <source> at " << source.GetLocation();
            return DynamicArray<Type>();
        }

        const DataFlow::Accessor::Param* param = nullptr;
        for (unsigned c=0; c<accessor->GetParamCount(); ++c)
            if (Is(accessor->GetParam(c)._name, paramName)) {
                param = &accessor->GetParam(c);
                break;
            }
        if (!param) {
            LogWarning << "Expecting parameter with name " << paramName << " in <source> at " << source.GetLocation();
            return DynamicArray<Type>();
        }

        auto offset = accessor->GetOffset() + param->_offset;
        auto stride = accessor->GetStride();
        auto count = accessor->GetCount();
        if ((offset + (count-1) * stride + 1) > source.GetCount()) {
            LogWarning << "Source array is too short for accessor in <source> at " << source.GetLocation();
            return DynamicArray<Type>();
        }

        DynamicArray<Type> result(std::make_unique<Type[]>(count), count);
        unsigned elementIndex = 0;
        unsigned lastWrittenPlusOne = 0;

        const auto* start = source.GetArrayData()._start;
        const auto* end = source.GetArrayData()._end;
        const auto* i = start;
        for (;;) {
            while (i < end && IsWhitespace(*i)) ++i;
            if (i == end) break;

            auto* elementStart = i;

                // we only actually need to parse the floating point number if
                // it's one we're interested in...
            if (elementIndex >= offset && (elementIndex-offset)%stride == 0) {
                i = FastParseElement(result[(elementIndex - offset) / stride], elementStart, end);
                lastWrittenPlusOne = ((elementIndex - offset) / stride)+1;
            } else {
                while (i < end && !IsWhitespace(*i)) ++i;
            }

            ++elementIndex;
        }

        for (unsigned c=lastWrittenPlusOne; c<count; ++c)
            result[c] = Type(0);

        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ControllerVertexIterator
    {
    public:
        unsigned GetNextInfluenceCount() const;
        void GetNextInfluences(
            unsigned influencesPerVertex,
            unsigned jointIndices[],
            float weights[]) const;

        ControllerVertexIterator(
            const SkinController& controller,
            const URIResolveContext& resolveContext);
        ~ControllerVertexIterator();

    protected:
        Section _vcount;
        Section _v;

        mutable utf8 const* _vcountI;
        mutable utf8 const* _vI;

        DynamicArray<float> _rawWeights;

        unsigned _jointIndexOffset;
        unsigned _weightIndexOffset;
        unsigned _bindStride;

        unsigned GetNextIndex() const;
    };

    inline unsigned ControllerVertexIterator::GetNextInfluenceCount() const
    {
        while (_vcountI < _vcount._end && IsWhitespace(*_vcountI)) ++_vcountI;
        if (_vcountI == _vcount._end) return 0;

        unsigned result = 0u;
        _vcountI = FastParseElement(result, _vcountI, _vcount._end);
        return result;
    }

    inline unsigned ControllerVertexIterator::GetNextIndex() const
    {
        while (_vI < _v._end && IsWhitespace(*_vI)) ++_vI;
        if (_vI == _v._end) return 0;

        unsigned result = 0u;
        _vI = FastParseElement(result, _vI, _v._end);
        return result;
    }

    void ControllerVertexIterator::GetNextInfluences(
        unsigned influencesPerVertex,
        unsigned jointIndices[], float weights[]) const
    {
        unsigned temp[AbsoluteMaxJointInfluenceCount * 4];
        assert((influencesPerVertex * _bindStride) < dimof(temp));
        for (unsigned c=0; c<influencesPerVertex * _bindStride; ++c)
            temp[c] = GetNextIndex();

        if (_jointIndexOffset != ~0u) {
            for (unsigned v=0; v<influencesPerVertex; ++v)
                jointIndices[v] = temp[v*_bindStride + _jointIndexOffset];
        } else {
            for (unsigned v=0; v<influencesPerVertex; ++v)
                jointIndices[v] = ~0u;
        }

        if (_weightIndexOffset != ~0u) {
            for (unsigned v=0; v<influencesPerVertex; ++v) {
                auto weightIndex = temp[v*_bindStride + _weightIndexOffset];
                assert(weightIndex < _rawWeights.size());
                weights[v] = _rawWeights[weightIndex];
            }
        } else {
            for (unsigned v=0; v<influencesPerVertex; ++v)
                weights[v] = 0.f;
        }
    }

    ControllerVertexIterator::ControllerVertexIterator(
        const SkinController& controller,
        const URIResolveContext& resolveContext)
    {
        _weightIndexOffset = ~0u;
        _jointIndexOffset = ~0u;

        auto weightInput = controller.GetInfluenceInputBySemantic(u("WEIGHT"));
        if (weightInput) {
            _weightIndexOffset = weightInput->_indexInPrimitive;
            auto weightSource = FindElement(
                GuidReference(weightInput->_source), resolveContext, 
                &IDocScopeIdResolver::FindSource);
            if (weightSource)
                _rawWeights = AsScalarArray<float>(*weightSource, u("WEIGHT"));
        }

        auto jointInput = controller.GetInfluenceInputBySemantic(u("JOINT"));
        if (jointInput)
            _jointIndexOffset = jointInput->_indexInPrimitive;

        _bindStride = 0;
        for (unsigned c=0; c<controller.GetInfluenceInputCount(); ++c)
            _bindStride = std::max(_bindStride, controller.GetInfluenceInput(c)._indexInPrimitive+1);

            // we have a fixed size array in GetNextInfluences(), so we need to limit the
            // value of _bindStride to something sensible. In normal cases, it should be 2.
            // But maybe some unusual exporters will add extra elements.
        if (_bindStride > 4)
            Throw(FormatException("Too many <input> elements within <vertex_weights> in controller", controller.GetLocation()));

        _vcount = controller.GetInfluenceCountPerVertexArray();
        _v = controller.GetInfluencesArray();

        _vcountI = _vcount._start;
        _vI = _v._start;
    }

    ControllerVertexIterator::~ControllerVertexIterator() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template <int WeightCount>
        class VertexWeightAttachment
    {
    public:
        uint8       _weights[WeightCount];            // go straight to compressed 8 bit value
        uint8       _jointIndex[WeightCount];
    };

    template <>
        class VertexWeightAttachment<0>
    {
    };

    template <int WeightCount>
        class VertexWeightAttachmentBucket
    {
    public:
        std::vector<uint16>                                 _vertexBindings;
        std::vector<VertexWeightAttachment<WeightCount>>    _weightAttachments;
    };

    template<unsigned WeightCount> 
        VertexWeightAttachment<WeightCount> BuildWeightAttachment(const uint8 weights[], const unsigned joints[], unsigned jointCount)
    {
        VertexWeightAttachment<WeightCount> attachment;
        std::fill(attachment._weights, &attachment._weights[dimof(attachment._weights)], 0);
        std::fill(attachment._jointIndex, &attachment._jointIndex[dimof(attachment._jointIndex)], 0);
        std::copy(weights, &weights[std::min(WeightCount,jointCount)], attachment._weights);
        std::copy(joints, &joints[std::min(WeightCount,jointCount)], attachment._jointIndex);
        return attachment;
    }

    template<> VertexWeightAttachment<0> BuildWeightAttachment(const uint8 weights[], const unsigned joints[], unsigned jointCount)
    {
        return VertexWeightAttachment<0>();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto Convert(   const SkinController& controller, 
                    const URIResolveContext& resolveContext,
                    const ImportConfiguration& cfg)
        -> RenderCore::ColladaConversion::UnboundSkinController
    {
        auto bindShapeMatrix = Identity<Float4x4>();
        ParseXMLList(&bindShapeMatrix(0,0), 16, controller.GetBindShapeMatrix());

            //
            //      If we have a mesh where there are many vertices with only 1 or 2
            //      weights, but others with 4, we could potentially split the
            //      skinning operation up into multiple parts.
            //
            //      This could be done in 2 ways:
            //          1. split the geometry into multiple meshes, with extra draw calls
            //          2. do skinning in a preprocessing step before Draw
            //                  Ie; multiple skin -> to vertex buffer steps
            //                  then a single draw call...
            //              That would be efficient if writing to a GPU vertex buffer was
            //              very fast (it would also help reduce shader explosion).
            //
            //      If we were using type 2, it might be best to separate the animated parts
            //      of the vertex from the main vertex buffer. So texture coordinates and vertex
            //      colors will be static, so left in a separate buffer. But positions (and possibly
            //      normals and tangent frames) might be animated. So they could be held separately.
            //
            //      It might be handy to have a vertex buffer with just the positions. This could
            //      be used for pre-depth passes, etc.
            //
            //      Option 2 would be an efficient mode in multiple-pass rendering (because we
            //      apply the skinning only once, even if the object is rendered multiple times).
            //      But we need lots of temporary buffer space. Apparently in D3D11.1, we can 
            //      output to a buffer as a side-effect of vertex transformations, also. So a 
            //      first pass vertex shader could do depth-prepass and generate skinned
            //      positions for the second pass. (but there are some complications... might be
            //      worth experimenting with...)
            //
            //
            //      Let's create a number of buckets based on the number of weights attached
            //      to that vertex. Ideally we want a lot of vertices in the smaller buckets, 
            //      and few in the high buckets.
            //

        VertexWeightAttachmentBucket<4> bucket4;
        VertexWeightAttachmentBucket<2> bucket2;
        VertexWeightAttachmentBucket<1> bucket1;
        VertexWeightAttachmentBucket<0> bucket0;

        size_t vertexCount = controller.GetVerticesWithWeightsCount();
        if (vertexCount >= std::numeric_limits<uint16>::max()) {
            Throw(FormatException(
                "Exceeded maximum number of vertices supported by skinning controller", controller.GetLocation()));
        }
        
        ControllerVertexIterator vIterator(controller, resolveContext);

        std::vector<uint32> vertexPositionToBucketIndex;
        vertexPositionToBucketIndex.reserve(vertexCount);

        for (size_t vertex=0; vertex<vertexCount; ++vertex) {

            auto influenceCount = vIterator.GetNextInfluenceCount();
            if (influenceCount > AbsoluteMaxJointInfluenceCount)
                Throw(FormatException(
                    "Too many influences per vertex in skinning controller", controller.GetLocation()));

                //
                //      Sometimes the input data has joints attached at very low weight
                //      values. In these cases it's better to just ignore the influence.
                //
                //      So we need to calculate the normalized weights for all of the
                //      influences, first -- and then strip out the unimportant ones.
                //
            float weights[AbsoluteMaxJointInfluenceCount];
            unsigned jointIndices[AbsoluteMaxJointInfluenceCount];
            vIterator.GetNextInfluences(influenceCount, jointIndices, weights);

            const float minWeightThreshold = 8.f / 255.f;
            float totalWeightValue = 0.f;
            for (size_t c=0; c<influenceCount;) {
                if (weights[c] < minWeightThreshold) {
                    std::move(&weights[c+1],        &weights[influenceCount],       &weights[c]);
                    std::move(&jointIndices[c+1],   &jointIndices[influenceCount],  &jointIndices[c]);
                    --influenceCount;
                } else {
                    totalWeightValue += weights[c];
                    ++c;
                }
            }

            uint8 normalizedWeights[AbsoluteMaxJointInfluenceCount];
            for (size_t c=0; c<influenceCount; ++c) {
                normalizedWeights[c] = (uint8)(Clamp(weights[c] / totalWeightValue, 0.f, 1.f) * 255.f + .5f);
            }

                //
                // \todo -- should we sort influcences by the strength of the influence, or by the joint
                //          index?
                //
                //          Sorting by weight could allow us to decrease the number of influences
                //          calculated smoothly.
                //
                //          Sorting by joint index might mean that adjacent vertices are more frequently
                //          calculating the same joint.
                //

            #if defined(_DEBUG)     // double check to make sure no joint is referenced twice!
                for (size_t c=1; c<influenceCount; ++c) {
                    assert(std::find(jointIndices, &jointIndices[c], jointIndices[c]) == &jointIndices[c]);
                }
            #endif

            if (influenceCount >= 3) {
                if (influenceCount > 4) {
                    LogAlwaysWarning 
                        << "Warning -- Exceeded maximum number of joints affecting a single vertex in skinning controller " 
                        << controller.GetLocation() 
                        << "Only 4 joints can affect any given single vertex.";

                        // (When this happens, only use the first 4, and ignore the others)
                    LogAlwaysWarningF("After filtering:\n");
                    for (size_t c=0; c<influenceCount; ++c) {
                        LogAlwaysWarningF("  [%i] Weight: %i Joint: %i", c, normalizedWeights[c], jointIndices[c]);
                    }
                }

                    // (we could do a separate bucket for 3, if it was useful)
                vertexPositionToBucketIndex.push_back((0<<16) | (uint32(bucket4._vertexBindings.size())&0xffff));
                bucket4._vertexBindings.push_back(uint16(vertex));
                bucket4._weightAttachments.push_back(BuildWeightAttachment<4>(normalizedWeights, jointIndices, (unsigned)influenceCount));
            } else if (influenceCount == 2) {
                vertexPositionToBucketIndex.push_back((1<<16) | (uint32(bucket2._vertexBindings.size())&0xffff));
                bucket2._vertexBindings.push_back(uint16(vertex));
                bucket2._weightAttachments.push_back(BuildWeightAttachment<2>(normalizedWeights, jointIndices, (unsigned)influenceCount));
            } else if (influenceCount == 1) {
                vertexPositionToBucketIndex.push_back((2<<16) | (uint32(bucket1._vertexBindings.size())&0xffff));
                bucket1._vertexBindings.push_back(uint16(vertex));
                bucket1._weightAttachments.push_back(BuildWeightAttachment<1>(normalizedWeights, jointIndices, (unsigned)influenceCount));
            } else {
                vertexPositionToBucketIndex.push_back((3<<16) | (uint32(bucket0._vertexBindings.size())&0xffff));
                bucket0._vertexBindings.push_back(uint16(vertex));
                bucket0._weightAttachments.push_back(BuildWeightAttachment<0>(normalizedWeights, jointIndices, (unsigned)influenceCount));
            }
        }
            
        UnboundSkinController::Bucket b4;
        b4._vertexBindings = std::move(bucket4._vertexBindings);
        b4._weightCount = 4;
        b4._vertexBufferSize = bucket4._weightAttachments.size() * sizeof(VertexWeightAttachment<4>);
        b4._vertexBufferData = std::make_unique<uint8[]>(b4._vertexBufferSize);
        XlCopyMemory(b4._vertexBufferData.get(), AsPointer(bucket4._weightAttachments.begin()), b4._vertexBufferSize);
        b4._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8G8B8A8_UNORM, 1, 0));
        b4._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8G8B8A8_UINT, 1, 4));

        UnboundSkinController::Bucket b2;
        b2._vertexBindings = std::move(bucket2._vertexBindings);
        b2._weightCount = 2;
        b2._vertexBufferSize = bucket2._weightAttachments.size() * sizeof(VertexWeightAttachment<2>);
        b2._vertexBufferData = std::make_unique<uint8[]>(b2._vertexBufferSize);
        XlCopyMemory(b2._vertexBufferData.get(), AsPointer(bucket2._weightAttachments.begin()), b2._vertexBufferSize);
        b2._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8G8_UNORM, 1, 0));
        b2._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8G8_UINT, 1, 2));

        UnboundSkinController::Bucket b1;
        b1._vertexBindings = std::move(bucket1._vertexBindings);
        b1._weightCount = 1;
        b1._vertexBufferSize = bucket1._weightAttachments.size() * sizeof(VertexWeightAttachment<1>);
        b1._vertexBufferData = std::make_unique<uint8[]>(b1._vertexBufferSize);
        XlCopyMemory(b1._vertexBufferData.get(), AsPointer(bucket1._weightAttachments.begin()), b1._vertexBufferSize);
        b1._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8_UNORM, 1, 0));
        b1._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8_UINT, 1, 1));

        UnboundSkinController::Bucket b0;
        b0._vertexBindings = std::move(bucket0._vertexBindings);
        b0._weightCount = 0;
        b0._vertexBufferSize = bucket0._weightAttachments.size() * sizeof(VertexWeightAttachment<0>);
        if (b0._vertexBufferSize) {
            b0._vertexBufferData = std::make_unique<uint8[]>(b0._vertexBufferSize);
            XlCopyMemory(b0._vertexBufferData.get(), AsPointer(bucket0._weightAttachments.begin()), b0._vertexBufferSize);
        }

        auto inverseBindMatrices = GetInverseBindMatrices(controller, resolveContext);
        auto jointNames = GetJointNames(controller, resolveContext);
        GuidReference ref(controller.GetBaseMesh());
        return UnboundSkinController(
            std::move(b4), std::move(b2), std::move(b1), std::move(b0),
            std::move(inverseBindMatrices), bindShapeMatrix, 
            std::move(jointNames), 
            ObjectGuid(ref._id, ref._fileHash),
            std::move(vertexPositionToBucketIndex));
    }

}

