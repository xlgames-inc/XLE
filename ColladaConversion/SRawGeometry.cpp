// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS
#pragma warning(disable:4244)		// '=': conversion from 'const unsigned int' to 'unsigned short', possible loss of data

#include "SRawGeometry.h"
#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"
#include "ConversionConfig.h"
#include "../RenderCore/GeoProc/NascentRawGeometry.h"
#include "../RenderCore/GeoProc/NascentAnimController.h"
#include "../RenderCore/GeoProc/GeometryAlgorithm.h"
#include "../RenderCore/GeoProc/GeoProcUtil.h"
#include "../RenderCore/GeoProc/MeshDatabase.h"
#include "../RenderCore/Assets/AssetUtils.h"
#include "../RenderCore/Types.h"      // for Topology...!
#include "../RenderCore/Format.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <map>
#include <set>

namespace ColladaConversion
{
    using namespace RenderCore;
    using namespace RenderCore::Assets::GeoProc;

    static const std::string DefaultSemantic_Weights         = "WEIGHTS";
    static const std::string DefaultSemantic_JointIndices    = "JOINTINDICES";

    static const unsigned AbsoluteMaxJointInfluenceCount = 256;

    class VertexSourceData : public IVertexSourceData
    {
    public:
        IteratorRange<const void*> GetData() const;
        Format GetFormat() const { return _dataFormat; }
        size_t GetStride() const { return _stride; }
        size_t GetCount() const { return _count; }
        ProcessingFlags::BitField GetProcessingFlags() const { return _processingFlags; }
        FormatHint::BitField GetFormatHint() const { return _formatHint; }

        VertexSourceData(const DataFlow::Source& source, ProcessingFlags::BitField processingFlags);
        ~VertexSourceData();

    protected:
        Format _dataFormat;
        size_t _stride;
        size_t _offset;
        size_t _count;
        FormatHint::BitField _formatHint;
        ProcessingFlags::BitField _processingFlags;

        ::Assets::Blob _rawData;
    };

    IteratorRange<const void*> VertexSourceData::GetData() const
    {
		return MakeIteratorRange(
			PtrAdd(AsPointer(_rawData->cbegin()), _offset),
			AsPointer(_rawData->cend()));
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

        unsigned parsedTypeSize;
        Format finalFormat;
        if (sourceType == DataFlow::ArrayType::Float) {
            switch (paramCount) {
            case 1: finalFormat = Format::R32_FLOAT; break;
            case 2: finalFormat = Format::R32G32_FLOAT; break;
            case 3: finalFormat = Format::R32G32B32_FLOAT; break;
            default: finalFormat = Format::R32G32B32A32_FLOAT; break;
            }
            parsedTypeSize = sizeof(float);
        } else if (sourceType == DataFlow::ArrayType::Int) {
            switch (paramCount) {
            case 1: finalFormat = Format::R32_SINT; break;
            case 2: finalFormat = Format::R32G32_SINT; break;
            case 3: finalFormat = Format::R32G32B32_SINT; break;
            default: finalFormat = Format::R32G32B32A32_SINT; break;
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
        } 
		const bool renormalize = false;
		if (renormalize) {
			if (semantic.find(u("TEXTANGENT")) != npos || semantic.find(u("textangent")) != npos) {
				return ProcessingFlags::Renormalize;
			} else if (semantic.find(u("TEXBITANGENT")) != npos || semantic.find(u("texbitangent")) != npos) {
				return ProcessingFlags::Renormalize;
			} else if (semantic.find(u("NORMAL")) != npos || semantic.find(u("normal")) != npos) {
				return ProcessingFlags::Renormalize;
			}
		}
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
                if (_cfg->GetVertexSemanticBindings().IsSuppressed(semantic))
                    return ~size_t(0);
                semanticStr = _cfg->GetVertexSemanticBindings().AsNative(semantic);
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
        Topology				_topology = (Topology)0;
        Section					_materialBinding;
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
        drawCall._materialBinding = geoPrim.GetMaterialBinding();
        drawCall._topology = Topology::TriangleList;
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
        drawCall._materialBinding = geoPrim.GetMaterialBinding();
        drawCall._topology = Topology::TriangleList;
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
        drawCall._materialBinding = geoPrim.GetMaterialBinding();
        drawCall._topology = Topology::TriangleList;
        return std::move(drawCall);
    }

    static std::shared_ptr<MeshDatabase> BuildMeshDatabase(
		std::vector<WorkingDrawOperation>& drawOperations,
        const MeshGeometry& mesh, 
        const URIResolveContext& pubEles, 
        const ImportConfiguration& cfg)
    {
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

        for (size_t prim=0; prim<mesh.GetPrimitivesCount(); ++prim) {
            const auto& geoPrim = mesh.GetPrimitives(prim);
            
            WorkingPrimitive workingPrim;
            workingPrim._primitiveStride = 0;

            auto inputCount = geoPrim.GetInputCount();
            for (size_t q=0; q<inputCount; ++q) {
                const auto& input = geoPrim.GetInput(q);

                    // the "source" can be either a DataFlow::Source, or
                    // it can be a VertexInputs

                bool boundSource = false;
                    
                GuidReference ref(input._source);
                const auto* f = pubEles.FindFile(ref._fileHash);
                if (f) {

                    const auto* dataSource = f->FindSource(ref._id);
                    if (dataSource) {

                        workingPrim._inputs.push_back(
                            WorkingPrimitive::EleRef
                            {
                                composingVertex.FindOrCreateElement(*dataSource, input._semantic, input._semanticIndex),
                                input._indexInPrimitive
                            });
                        boundSource = true;

                    } else {

                        const auto* extraInputs = f->FindVertexInputs(ref._id);
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
                    Log(Warning) << "Couldn't find source for geometry input. Source name: " << input._source << " in geometry " << mesh.GetName() << std::endl;
                }

                    // we must adjust the primitive stride, even if we couldn't properly resolve
                    // the input source
                workingPrim._primitiveStride = std::max(input._indexInPrimitive+1, workingPrim._primitiveStride);
            }

            atLeastOneInput |= workingPrim._inputs.size() > 0;
            workingPrims.push_back(workingPrim);
        }

        if (!atLeastOneInput) {
            Log(Warning) << "Geometry object with no valid vertex inputs: " << mesh.GetName() << std::endl;
            return nullptr;
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
            return nullptr;
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

		return database;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	ConvertedMeshGeometry Convert(const MeshGeometry& mesh, const URIResolveContext& pubEles, const ImportConfiguration& cfg)
	{
		if (!mesh.GetPrimitivesCount()) {
            Log(Warning) << "Geometry object with no primitives: " << mesh.GetName() << std::endl;
			return {};
        }

		std::vector<WorkingDrawOperation> drawOperations;
		auto database = BuildMeshDatabase(drawOperations, mesh, pubEles, cfg);
		if (!database)
			return {};

		std::vector<RenderCore::Assets::GeoProc::NascentModel::DrawCallDesc> finalDrawOperations;
		std::vector<uint64_t> materialBindingSymbols;
        finalDrawOperations.reserve(drawOperations.size());
		materialBindingSymbols.reserve(drawOperations.size());

        size_t finalIndexCount = 0;
        for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
            assert(i->_topology == Topology::TriangleList);  // tangent generation assumes triangle list currently... We need to modify GenerateNormalsAndTangents() to support other types
            finalDrawOperations.push_back({(unsigned)finalIndexCount, (unsigned)i->_indexBuffer.size(), i->_topology});
			materialBindingSymbols.push_back(Hash64(i->_materialBinding.begin(), i->_materialBinding.end()));
            finalIndexCount += i->_indexBuffer.size();
        }

            //  \todo -- sort by material id?

        Format indexFormat;
        std::vector<uint8_t> finalIndexBuffer;
                
        if (finalIndexCount < 0xffff) {

            size_t accumulatingIndexCount = 0;
            indexFormat = Format::R16_UINT;
            finalIndexBuffer.resize(finalIndexCount*sizeof(uint16_t));
            for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
                size_t count = i->_indexBuffer.size();
                std::copy(
                    i->_indexBuffer.begin(), i->_indexBuffer.end(),
                    &((uint16*)finalIndexBuffer.data())[accumulatingIndexCount]);
                accumulatingIndexCount += count;
            }
            assert(accumulatingIndexCount==finalIndexCount);

        } else {

            size_t accumulatingIndexCount = 0;
            indexFormat = Format::R32_UINT;
            finalIndexBuffer.resize(finalIndexCount*sizeof(uint32_t));
            for (auto i=drawOperations.cbegin(); i!=drawOperations.cend(); ++i) {
                size_t count = i->_indexBuffer.size();
                std::copy(
                    i->_indexBuffer.begin(), i->_indexBuffer.end(),
                    &((uint32*)finalIndexBuffer.data())[accumulatingIndexCount]);
                accumulatingIndexCount += count;
            }
            assert(accumulatingIndexCount==finalIndexCount);

        }

		auto vertexMapping = database->BuildUnifiedVertexIndexToPositionIndex();

		return ConvertedMeshGeometry{
			NascentModel::GeometryBlock {
				database,
				std::move(finalDrawOperations),
				std::vector<unsigned>{vertexMapping.get(), &vertexMapping[database->GetUnifiedVertexCount()]},
				std::move(finalIndexBuffer),
				indexFormat
			},
			materialBindingSymbols
		};
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::vector<Float4x4> GetInverseBindMatrices(
        const SkinController& skinController,
        const URIResolveContext& resolveContext,
		IteratorRange<const unsigned*> remapping)
    {
        auto invBindInput = skinController.GetJointInputs().FindInputBySemantic(u("INV_BIND_MATRIX"));
		if (!invBindInput) return {};

        auto* invBindSource = FindElement(
            GuidReference(invBindInput->_source), resolveContext, 
            &IDocScopeIdResolver::FindSource);
		if (!invBindSource) return {};

        if (invBindSource->GetType() != DataFlow::ArrayType::Float)
			return {};

        auto* commonAccessor = invBindSource->FindAccessorForTechnique();
		if (!commonAccessor) return {};

        if (    commonAccessor->GetParamCount() != 1
            || !Is(commonAccessor->GetParam(0)._type, u("float4x4"))) {
            Log(Warning) << "Expecting inverse bind matrices expressed as float4x4 elements. These inverse bind elements will be ignored. In <source> at " << invBindSource->GetLocation() << std::endl;
			return {};
        }

        auto count = commonAccessor->GetCount();
        if (!count)
			return {};

		std::vector<Float4x4> result;
		result.reserve(count);

            // parse in the array of raw floats
        auto rawFloatCount = invBindSource->GetCount();
        auto rawFloats = std::make_unique<float[]>(rawFloatCount);
        ParseXMLList(rawFloats.get(), (unsigned)rawFloatCount, invBindSource->GetArrayData());

        for (unsigned c=0; c<count; ++c) {
            auto r = c * commonAccessor->GetStride();
			Float4x4 transform;
            if ((r + 16) <= rawFloatCount) {
                transform = MakeFloat4x4(
                    rawFloats[r+ 0], rawFloats[r+ 1], rawFloats[r+ 2], rawFloats[r+ 3],
                    rawFloats[r+ 4], rawFloats[r+ 5], rawFloats[r+ 6], rawFloats[r+ 7],
                    rawFloats[r+ 8], rawFloats[r+ 9], rawFloats[r+10], rawFloats[r+11],
                    rawFloats[r+12], rawFloats[r+13], rawFloats[r+14], rawFloats[r+15]);
            } else
                transform = Identity<Float4x4>();

			if (c < remapping.size() && remapping[c] != ~0u) {
				if (result.size() <= remapping[c])
					result.resize(remapping[c]+1);
				result[remapping[c]] = transform;
			}
        }

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::vector<std::string> GetJointNames(
        const SkinController& controller,
        const URIResolveContext& resolveContext,
		IteratorRange<const unsigned*> remapping)
    {
        std::vector<std::string> result;

            // we're expecting an input called "JOINT" that contains the names of joints
            // These names should match the "sid" of nodes within the hierachy of nodes
            // beneath our "skeleton"
        auto jointInput = controller.GetJointInputs().FindInputBySemantic(u("JOINT"));
		if (!jointInput) return {};

        auto* jointSource = FindElement(
            GuidReference(jointInput->_source), resolveContext, 
            &IDocScopeIdResolver::FindSource);
		if (!jointSource) return {};

        if (    jointSource->GetType() != DataFlow::ArrayType::Name
            &&  jointSource->GetType() != DataFlow::ArrayType::SidRef)
			return {};

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

			if (elementIndex < remapping.size() && remapping[elementIndex] != ~0u) {
				if (result.size() <= remapping[elementIndex])
					result.resize(remapping[elementIndex]+1);
				result[remapping[elementIndex]] = std::string((const char*)elementStart, (const char*)i);
			}
            // result.push_back(std::string((const char*)elementStart, (const char*)i));
			++elementIndex;
            if (i == arrayData._end || elementIndex == count) break;
        }

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type>
        DynamicArray<Type> AsScalarArray(const DataFlow::Source& source, const utf8 paramName[])
    {
        auto* accessor = source.FindAccessorForTechnique();
        if (!accessor) {
            Log(Warning) << "Expecting common access in <source> at " << source.GetLocation() << std::endl;
            return DynamicArray<Type>();
        }

        const DataFlow::Accessor::Param* param = nullptr;
        for (unsigned c=0; c<accessor->GetParamCount(); ++c)
            if (Is(accessor->GetParam(c)._name, paramName)) {
                param = &accessor->GetParam(c);
                break;
            }
        // sometimes there a source might have just a single unnamed param. In these cases, we must select that one.
        if (!param && accessor->GetParamCount() == 1 && accessor->GetParam(0)._name.IsEmpty()) {
            param = &accessor->GetParam(0);
        }
        if (!param) {
            Log(Warning) << "Expecting parameter with name " << paramName << " in <source> at " << source.GetLocation() << std::endl;
            return DynamicArray<Type>();
        }

        auto offset = accessor->GetOffset() + param->_offset;
        auto stride = accessor->GetStride();
        auto count = accessor->GetCount();
        if ((offset + (count-1) * stride + 1) > source.GetCount()) {
            Log(Warning) << "Source array is too short for accessor in <source> at " << source.GetLocation() << std::endl;
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

    auto Convert(   const SkinController& controller, 
                    const URIResolveContext& resolveContext,
                    const ImportConfiguration& cfg)
        -> RenderCore::Assets::GeoProc::UnboundSkinController
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
                    Log(Warning)
                        << "Warning -- Exceeded maximum number of joints affecting a single vertex in skinning controller " 
                        << controller.GetLocation() 
                        << ". Only 4 joints can affect any given single vertex."
						<< std::endl;

                        // (When this happens, only use the first 4, and ignore the others)
					Log(Warning) << "After filtering:" << std::endl;
                    for (size_t c=0; c<influenceCount; ++c) {
						Log(Warning) << "  [" << c << "] Weight: " << normalizedWeights[c] << " Joint: " << jointIndices[c] << std::endl;
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

		std::vector<unsigned> jointUsage;
		jointUsage.resize(256);
		AccumulateJointUsage(bucket1, jointUsage);
		AccumulateJointUsage(bucket2, jointUsage);
		AccumulateJointUsage(bucket4, jointUsage);

		std::vector<unsigned> jointIndexRemapping;
		unsigned finalJointIndexCount = 0;
		for (unsigned c=0; c<jointUsage.size(); ++c) {
			if (jointUsage[c]) {
				if (jointIndexRemapping.size() <= c)
					jointIndexRemapping.resize(c+1, ~0u);
				jointIndexRemapping[c] = finalJointIndexCount;
				++finalJointIndexCount;
			}
		}
		RemapJointIndices(bucket1, MakeIteratorRange(jointIndexRemapping));
		RemapJointIndices(bucket2, MakeIteratorRange(jointIndexRemapping));
		RemapJointIndices(bucket4, MakeIteratorRange(jointIndexRemapping));
            
        UnboundSkinController::Bucket b4;
        b4._vertexBindings = std::move(bucket4._vertexBindings);
        b4._weightCount = 4;
        b4._vertexBufferSize = bucket4._weightAttachments.size() * sizeof(VertexWeightAttachment<4>);
        b4._vertexBufferData = std::make_unique<uint8[]>(b4._vertexBufferSize);
        XlCopyMemory(b4._vertexBufferData.get(), AsPointer(bucket4._weightAttachments.begin()), b4._vertexBufferSize);
        b4._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_Weights, 0, Format::R8G8B8A8_UNORM, 1, 0));
        b4._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_JointIndices, 0, Format::R8G8B8A8_UINT, 1, 4));

        UnboundSkinController::Bucket b2;
        b2._vertexBindings = std::move(bucket2._vertexBindings);
        b2._weightCount = 2;
        b2._vertexBufferSize = bucket2._weightAttachments.size() * sizeof(VertexWeightAttachment<2>);
        b2._vertexBufferData = std::make_unique<uint8[]>(b2._vertexBufferSize);
        XlCopyMemory(b2._vertexBufferData.get(), AsPointer(bucket2._weightAttachments.begin()), b2._vertexBufferSize);
        b2._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_Weights, 0, Format::R8G8_UNORM, 1, 0));
        b2._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_JointIndices, 0, Format::R8G8_UINT, 1, 2));

        UnboundSkinController::Bucket b1;
        b1._vertexBindings = std::move(bucket1._vertexBindings);
        b1._weightCount = 1;
        b1._vertexBufferSize = bucket1._weightAttachments.size() * sizeof(VertexWeightAttachment<1>);
        b1._vertexBufferData = std::make_unique<uint8[]>(b1._vertexBufferSize);
        XlCopyMemory(b1._vertexBufferData.get(), AsPointer(bucket1._weightAttachments.begin()), b1._vertexBufferSize);
        b1._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_Weights, 0, Format::R8_UNORM, 1, 0));
        b1._vertexInputLayout.push_back(InputElementDesc(DefaultSemantic_JointIndices, 0, Format::R8_UINT, 1, 1));

        UnboundSkinController::Bucket b0;
        b0._vertexBindings = std::move(bucket0._vertexBindings);
        b0._weightCount = 0;
        b0._vertexBufferSize = bucket0._weightAttachments.size() * sizeof(VertexWeightAttachment<0>);
        if (b0._vertexBufferSize) {
            b0._vertexBufferData = std::make_unique<uint8[]>(b0._vertexBufferSize);
            XlCopyMemory(b0._vertexBufferData.get(), AsPointer(bucket0._weightAttachments.begin()), b0._vertexBufferSize);
        }

        auto inverseBindMatrices = GetInverseBindMatrices(controller, resolveContext, MakeIteratorRange(jointIndexRemapping));
        auto jointNames = GetJointNames(controller, resolveContext, MakeIteratorRange(jointIndexRemapping));
        GuidReference ref(controller.GetBaseMesh());
        return UnboundSkinController(
            std::move(b4), std::move(b2), std::move(b1), std::move(b0),
            std::move(inverseBindMatrices), bindShapeMatrix, 
            std::move(jointNames), 
            NascentObjectGuid(ref._id, ref._fileHash),
            std::move(vertexPositionToBucketIndex));
    }

}

