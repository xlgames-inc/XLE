// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainConfig.h"
#include "Terrain.h"
#include "TerrainMaterial.h"
#include "TerrainScaffold.h"
#include "../Assets/Assets.h"
#include "../Assets/AssetServices.h"
#include "../Assets/InvalidAssetManager.h"
#include "../Math/Transformations.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Conversion.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/StringFormat.h"
#include <cctype>

namespace SceneEngine
{
    static const char* KnownCoverageNames(TerrainCoverageId id)
    {
        const char* knownName = nullptr;
        switch (id) {
        case CoverageId_Heights: knownName = "height"; break;
        case CoverageId_AngleBasedShadows: knownName = "shadow"; break;
        case CoverageId_ArchiveHeights: knownName = "archiveheights"; break;
        }
        return knownName;
    }

    void TerrainConfig::GetCellFilename(
        ::Assets::ResChar buffer[], unsigned bufferCount,
        UInt2 cellIndex, TerrainCoverageId fileType) const
    {
        if (_filenamesMode == Legacy) {
                // note -- cell xy flipped
                //      This is a related to the terrain format used in Archeage
            switch (fileType) {
            case CoverageId_Heights:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%scells/%03i_%03i/client/terrain/heightmap.dat_new", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            case CoverageId_AngleBasedShadows:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%scells/%03i_%03i/client/terrain/shadow.ctc", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            case CoverageId_ArchiveHeights:
                _snprintf_s(buffer, bufferCount, _TRUNCATE, "%scells/%03i_%03i/client/terrain/heightmap.dat", 
                    _baseDir.c_str(), cellIndex[1], cellIndex[0]);
                break;
            default:
                buffer[0] = '\0';
                break;
            }
        } else if (_filenamesMode == XLE) {
            
            auto knownName = KnownCoverageNames(fileType);
            if (knownName) {
               _snprintf_s(
                    buffer, bufferCount, _TRUNCATE, "%sc%02i_%02i/%s.terr", 
                    _baseDir.c_str(), cellIndex[0], cellIndex[1], knownName);
            } else {
                _snprintf_s(
                    buffer, bufferCount, _TRUNCATE, "%sc%02i_%02i/%08x.terr", 
                    _baseDir.c_str(), cellIndex[0], cellIndex[1], fileType);
            }
        }
    }

    void TerrainConfig::GetUberSurfaceFilename(
        ::Assets::ResChar buffer[], unsigned bufferCount,
        const ::Assets::ResChar directory[],
        TerrainCoverageId fileType)
    {
        auto knownName = KnownCoverageNames(fileType);
        if (knownName) {
            _snprintf_s(
                buffer, bufferCount, _TRUNCATE, "%s/%s.uber", 
                directory, knownName);
        } else {
            _snprintf_s(
                buffer, bufferCount, _TRUNCATE, "%s/%08x.uber", 
                directory, fileType);
        }
    }

    UInt2 TerrainConfig::CellDimensionsInNodes() const
    {
        unsigned t = 1<<(_cellTreeDepth-1);
        return UInt2(t, t);
    }

    UInt2 TerrainConfig::NodeDimensionsInElements() const
    {
        return UInt2(_nodeDimsInElements, _nodeDimsInElements);
    }

    unsigned TerrainConfig::GetCoverageLayerCount() const 
    { 
        return unsigned(_coverageLayers.size()); 
    }

    auto TerrainConfig::GetCoverageLayer(unsigned index) const -> const CoverageLayer&
    { 
        return _coverageLayers[index]; 
    }

    static ::Assets::rstring FormatBaseDir(const ::Assets::ResChar input[])
    {
        // format the input directory name
        //  * all lower case
        //  * only use '/' separators
        //  * must end in a slash (unless empty)
        ::Assets::ResChar buffer[MaxPath];
        XlNormalizePath(buffer, dimof(buffer), input);
        auto len = XlStringLen(buffer);
        std::transform(buffer, &buffer[len], buffer, &std::tolower);
        if (len && buffer[len-1] != '/' && (len+1) < dimof(buffer)) {
            buffer[len] = '/';
            buffer[len+1] = '\0';
        }
        return buffer;
    }

    TerrainConfig::TerrainConfig(
		const ::Assets::ResChar baseDir[], UInt2 cellCount,
        Filenames filenamesMode, 
        unsigned nodeDimsInElements, unsigned cellTreeDepth, 
        unsigned nodeOverlap, float elementSpacing)
    : _baseDir(FormatBaseDir(baseDir)), _cellCount(cellCount), _filenamesMode(filenamesMode)
    , _nodeDimsInElements(nodeDimsInElements), _cellTreeDepth(cellTreeDepth), _nodeOverlap(nodeOverlap) 
    , _elementSpacing(elementSpacing)
    {
        ::Assets::ResChar buffer[MaxPath];
        const auto* fn = "terraintextures/textures.txt";
        XlConcatPath(buffer, dimof(buffer), _baseDir.c_str(), fn, &fn[XlStringLen(fn)]);
        _textureCfgName = buffer;
    }

    TerrainConfig::TerrainConfig(const ::Assets::ResChar baseDir[])
    : _baseDir(FormatBaseDir(baseDir)), _filenamesMode(XLE)
    , _cellCount(0,0), _nodeDimsInElements(32u), _nodeOverlap(2u)
    , _cellTreeDepth(5u), _elementSpacing(10.f)
    {
        size_t fileSize = 0;
        StringMeld<MaxPath> fn; fn << _baseDir << "world.cfg";
        auto sourceFile = LoadFileAsMemoryBlock(fn, &fileSize);
        
        TRY
        {
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));
            Document<utf8> doc(formatter);

            auto c = doc.Element(u("TerrainConfig"));
            if (c) {
                auto filenames = c.AttributeOrEmpty(u("Filenames"));
                if (!XlCompareStringI(filenames.c_str(), u("Legacy"))) { _filenamesMode = Legacy; }

                _nodeDimsInElements = c(u("NodeDims"), _nodeDimsInElements);
                _cellTreeDepth      = c(u("CellTreeDepth"), _cellTreeDepth);
                _nodeOverlap        = c(u("NodeOverlap"), _nodeOverlap);
                _elementSpacing     = c(u("ElementSpacing"), _elementSpacing);
                _cellCount          = c(u("CellCount"), _cellCount);

                auto coverage = c.Element(u("Coverage"));
                if (coverage) {
                    for (auto l = coverage.FirstChild(); l; l=l.NextSibling()) {
                        CoverageLayer layer;
                        layer._name         = l.Name();
                        layer._id           = l(u("Id"), 0);
                        layer._dimensions   = l(u("Dims"), UInt2(0, 0));
                        layer._format       = l(u("Format"), 35);
                        _coverageLayers.push_back(layer);
                    }
                }
            }

            {
                ::Assets::ResChar buffer[MaxPath];
                const auto* fn = "terraintextures/textures.txt";
                XlConcatPath(buffer, dimof(buffer), _baseDir.c_str(), fn, &fn[XlStringLen(fn)]);
                _textureCfgName = buffer;
            }

            ::Assets::Services::GetInvalidAssetMan().MarkValid(fn.get());
        } CATCH (const std::exception& e) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(fn.get(), e.what());
            throw;
        } CATCH(...) {
            ::Assets::Services::GetInvalidAssetMan().MarkInvalid(fn.get(), "Unknown error");
            throw;
        } CATCH_END
    }

    void TerrainConfig::Save()
    {
            // write this configuration file out to disk
            //  simple serialisation via the "Data" class
        
        auto output = OpenFileOutput(StringMeld<MaxPath>() << _baseDir << "world.cfg", "wb");
        OutputStreamFormatter formatter(*output);

        auto cfgEle = formatter.BeginElement(u("TerrainConfig"));
        if (_filenamesMode == Legacy)   { Serialize(formatter, u("Filenames"), u("Legacy")); }
        else                            { Serialize(formatter, u("Filenames"), u("XLE")); }

        Serialize(formatter, u("NodeDims"), _nodeDimsInElements);
        Serialize(formatter, u("CellTreeDepth"), _cellTreeDepth);
        Serialize(formatter, u("NodeOverlap"), _nodeOverlap);
        Serialize(formatter, u("ElementSpacing"), _elementSpacing);
        Serialize(formatter, u("CellCount"), _cellCount);

        auto covEle = formatter.BeginElement(u("Coverage"));
        for (auto l=_coverageLayers.cbegin(); l!=_coverageLayers.cend(); ++l) {
            auto ele = formatter.BeginElement(l->_name);
            Serialize(formatter, u("Id"), l->_id);
            Serialize(formatter, u("Dims"), l->_dimensions);
            Serialize(formatter, u("Format"), l->_format);
            formatter.EndElement(ele);
        }
        formatter.EndElement(covEle);
        formatter.EndElement(cfgEle);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    Float4x4 TerrainCoordinateSystem::CellBasedToWorld() const
    {
        return AsFloat4x4(
            ScaleTranslation(
                Float3(_cellSizeInMeters, _cellSizeInMeters, 1.f), _terrainOffset));
    }

    Float4x4 TerrainCoordinateSystem::WorldToCellBased() const
    {
        return AsFloat4x4(
            ScaleTranslation(
                Float3(1.f/_cellSizeInMeters, 1.f/_cellSizeInMeters, 1.f), -_terrainOffset/_cellSizeInMeters));
    }

    Float3      TerrainCoordinateSystem::TerrainOffset() const { return _terrainOffset; }
    void        TerrainCoordinateSystem::SetTerrainOffset(const Float3& newOffset) { _terrainOffset = newOffset; }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static UInt2x3 MakeUInt2x3(
        unsigned m00, unsigned m01, unsigned m02,
        unsigned m10, unsigned m11, unsigned m12)
    {
        UInt2x3 result;
        result(0,0) = m00;
        result(0,1) = m01;
        result(0,2) = m02;
        result(1,0) = m10;
        result(1,1) = m11;
        result(1,2) = m12;
        return result;
    }

    UInt2x3 TerrainConfig::CellBasedToCoverage(TerrainCoverageId coverageId) const
    {
        if (coverageId == CoverageId_Heights) {
            auto t = NodeDimensionsInElements();
            auto t2 = CellDimensionsInNodes();
            return MakeUInt2x3(
                t[0] * t2[0], 0u, 0u,
                0u, t[1]*t2[1], 0u);
        } else {
            for (auto l=_coverageLayers.cbegin(); l!=_coverageLayers.cend(); ++l)
                if (l->_id == coverageId) {
                    auto t = l->_dimensions;
                    auto t2 = CellDimensionsInNodes();
                    return MakeUInt2x3(
                        t[0] * t2[0], 0u, 0u,
                        0u, t[1]*t2[1], 0u);
                }
        }

        return MakeUInt2x3(
            1u, 0u, 0u,
            0u, 1u, 0u);
    }

    std::pair<float, float> CalculateMinAndMaxHeights(const ::Assets::ResChar heightMapFilename[], ITerrainFormat& ioFormat)
    {
            //  Calculate the bounding box. Note that we have to actually read the
            //  height map file to get this information. Perhaps we can cache this
            //  somewhere, to avoid having to load the scaffold for every cell on
            //  startup
        auto& heights = ioFormat.LoadHeights(heightMapFilename);
        float minHeight = FLT_MAX, maxHeight = -FLT_MAX;
        for (auto i=heights._nodes.cbegin(); i!=heights._nodes.cend(); ++i) {
            float zScale = (*i)->_localToCell(2, 2);
            float zOffset = (*i)->_localToCell(2, 3);
            minHeight = std::min(minHeight, zOffset);
            maxHeight = std::max(maxHeight, zOffset + zScale);
        }
        return std::make_pair(minHeight, maxHeight);
    }


    void TerrainCachedData::Write(OutputStream& stream) const
    {
        OutputStreamFormatter formatter(stream);
        auto heightRange = formatter.BeginElement(u("CellHeightRange"));
        for (auto l=_cells.cbegin(); l!=_cells.cend(); ++l) {
            auto cell = formatter.BeginElement("Cell");
            Serialize(formatter, "CellIndex", l->_cellIndex);
            Serialize(formatter, "HeightRange", l->_heightRange);
            formatter.EndElement(cell);
        }
        formatter.EndElement(heightRange);
    }

    TerrainCachedData::TerrainCachedData() {}
    TerrainCachedData::TerrainCachedData(const ::Assets::ResChar filename[])
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);

        InputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));
        Document<utf8> doc(formatter);

        auto heightRanges = doc.Element(u("CellHeightRange"));
        if (heightRanges) {
            for (auto child = heightRanges.FirstChild(); child; child=child.NextSibling()) {
                _cells.push_back(Cell
                    {
                        Deserialize(child, u("CellIndex"), UInt2(0,0)),
                        Deserialize(child, u("HeightRange"), std::make_pair(0.f, 0.f))
                    });
            }
        }
    }

    TerrainCachedData::TerrainCachedData(TerrainCachedData&& moveFrom)
    : _cells(std::move(moveFrom._cells)) {}

    TerrainCachedData& TerrainCachedData::operator=(TerrainCachedData&& moveFrom)
    {
        _cells = std::move(moveFrom._cells);
        return *this;
    }

    TerrainCachedData::TerrainCachedData(const TerrainConfig& cfg, ITerrainFormat& ioFormat)
    {
        const UInt2 cellMin(0, 0);
        const UInt2 cellMax = cfg._cellCount;
        auto cellBasedToTerrain = cfg.CellBasedToCoverage(CoverageId_Heights);

        _cells.reserve((cellMax[0] - cellMin[0]) * (cellMax[1] - cellMin[1]));

        for (unsigned cellY=cellMin[1]; cellY<cellMax[1]; ++cellY) {
            for (unsigned cellX=cellMin[0]; cellX<cellMax[0]; ++cellX) {

                ::Assets::ResChar heightMapFilename[MaxPath];
                cfg.GetCellFilename(
                    heightMapFilename, dimof(heightMapFilename), 
                    UInt2(cellX, cellY), CoverageId_Heights);

                Cell cell;
                cell._cellIndex = UInt2(cellX, cellY);
                cell._heightRange = CalculateMinAndMaxHeights(heightMapFilename, ioFormat);
                _cells.push_back(cell);
            }
        }
    }

    void WriteTerrainCachedData(
        Utility::OutputStream& stream,
        const TerrainConfig& cfg, 
        ITerrainFormat& format)
    {
        TerrainCachedData(cfg, format).Write(stream);
    }

    void WriteTerrainMaterialData(Utility::OutputStream& stream, const TerrainConfig& cfg)
    {
        if (!cfg._textureCfgName.empty()) {
            auto& scaffold = ::Assets::GetAssetDep<TerrainMaterialScaffold>(cfg._textureCfgName.c_str());
            scaffold.Write(stream);
        } else {
            auto& scaffold = ::Assets::GetAssetDep<TerrainMaterialScaffold>();
            scaffold.Write(stream);
        }
    }


        //////////////////////////////////////////////////////////////////////////////////////////
    static UInt2 TransformPointInt(const UInt2x3& trans, UInt2 input)
    {
        auto temp = trans * UInt3(input[0], input[1], 1);
        return UInt2(temp[0], temp[1]);
    }

    std::vector<PrimedCell> BuildPrimedCells(const TerrainConfig& cfg)
    {
            // we're going to calculate a list of cells refernced by this terrain config
            // the final result will be in "terrain coords"
            //      -- ie one unit per terrain element

        // cellMin[1] = std::min(cellMin[1], cfg._cellCount[1]);
        // cellMax[1] = std::min(cellMax[1], cfg._cellCount[1]);
        // cellMin[0] = std::min(cellMin[0], cfg._cellCount[0]);
        // cellMax[0] = std::min(cellMax[0], cfg._cellCount[0]);

        const UInt2 cellMin(0, 0);
        const UInt2 cellMax = cfg._cellCount;

        std::vector<PrimedCell> result;
        result.reserve((cellMax[0] - cellMin[0]) * (cellMax[1] - cellMin[1]));

        auto cellBasedToTerrain = cfg.CellBasedToCoverage(CoverageId_Heights);
        UInt2x3 cellBasedToCoverage[dimof(std::declval<PrimedCell>()._coverageUber)];
        for (unsigned c=0; c<cfg.GetCoverageLayerCount(); ++c)
            cellBasedToCoverage[c] = cfg.CellBasedToCoverage(cfg.GetCoverageLayer(c)._id);

        for (unsigned cellY=cellMin[1]; cellY<cellMax[1]; ++cellY) {
            for (unsigned cellX=cellMin[0]; cellX<cellMax[0]; ++cellX) {
                PrimedCell cell;
                cell._cellIndex = UInt2(cellX, cellY);
                cell._heightUber.first = TransformPointInt(cellBasedToTerrain, UInt2(cellX, cellY));
                cell._heightUber.second = TransformPointInt(cellBasedToTerrain, UInt2(cellX+1, cellY+1));
                
                cell._cellToTerrainCoords = Float4x4(
                    float(cell._heightUber.second[0] - cell._heightUber.first[0]), 0.f, 0.f, float(cell._heightUber.first[0]),
                    0.f, float(cell._heightUber.second[1] - cell._heightUber.first[1]), 0.f, float(cell._heightUber.first[1]),
                    0.f, 0.f, 1.f, 0.f,
                    0.f, 0.f, 0.f, 1.f);

                for (unsigned c=0; c<std::min(cfg.GetCoverageLayerCount(), (unsigned)dimof(cell._coverageUber)); ++c) {
                    cell._coverageUber[c].first = TransformPointInt(cellBasedToCoverage[c], UInt2(cellX, cellY));
                    cell._coverageUber[c].second = TransformPointInt(cellBasedToCoverage[c], UInt2(cellX+1, cellY+1));
                }

                result.push_back(cell);
            }
        }

        return std::move(result);
    }
}

