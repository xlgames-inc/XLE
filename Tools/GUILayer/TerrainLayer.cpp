// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainLayer.h"
#include "GUILayerUtil.h"
#include "MarshalString.h"
#include "../ToolsRig/TerrainConversion.h"

using namespace System;

namespace GUILayer
{

    VectorUInt2 TerrainConfig::CellDimsInNodes::get()
    {
        return AsVectorUInt2(_native->CellDimensionsInNodes());
    }

    VectorUInt2 TerrainConfig::NodeDims::get()
    {
        return AsVectorUInt2(_native->NodeDimensionsInElements());
    }

    VectorUInt2 TerrainConfig::CellCount::get() { return AsVectorUInt2(_native->_cellCount); }
    void TerrainConfig::CellCount::set(VectorUInt2 value) { _native->_cellCount = AsUInt2(value); }

    unsigned TerrainConfig::CellTreeDepth::get() { return _native->CellTreeDepth(); }
    unsigned TerrainConfig::NodeOverlap::get() { return _native->NodeOverlap(); }
    float TerrainConfig::ElementSpacing::get() { return _native->ElementSpacing(); }
    float TerrainConfig::SunPathAngle::get() { return _native->SunPathAngle(); }
    bool TerrainConfig::EncodedGradientFlags::get() { return _native->EncodedGradientFlags(); }

    unsigned TerrainConfig::CoverageLayerCount::get() { return _native->GetCoverageLayerCount(); }
    TerrainConfig::CoverageLayerDesc^ TerrainConfig::GetCoverageLayer(unsigned index)
    {
        return gcnew CoverageLayerDesc(_native->GetCoverageLayer(index));
    }

    void TerrainConfig::Add(CoverageLayerDesc^ layer)
    {
        return _native->AddCoverageLayer(layer->GetNative());
    }

    void TerrainConfig::InitCellCountFromUberSurface(String^ uberSurfaceDir)
    {
        _native->_cellCount = ToolsRig::GetCellCountFromUberSurface(
            clix::marshalString<clix::E_UTF8>(uberSurfaceDir).c_str(),
            _native->NodeDimensionsInElements(), _native->CellTreeDepth());
    }

    TerrainConfig::TerrainConfig(
        String^ cellsDirectory,
        unsigned nodeDimsInElements, unsigned cellTreeDepth, unsigned nodeOverlap,
        float elementSpacing, float sunPathAngle, bool encodedGradientFlags)
    {
        _native.reset(new NativeConfig(
            clix::marshalString<clix::E_UTF8>(cellsDirectory).c_str(),
            UInt2(0,0),
            nodeDimsInElements, cellTreeDepth, nodeOverlap,
            elementSpacing, sunPathAngle, encodedGradientFlags));
    }

    TerrainConfig::TerrainConfig(const NativeConfig& native)
    {
        _native.reset(new NativeConfig(native));
    }

    TerrainConfig::~TerrainConfig() {}



///////////////////////////////////////////////////////////////////////////////////////////////////

    String^ TerrainConfig::CoverageLayerDesc::Name::get()
    {
        return clix::marshalString<clix::E_UTF8>(_native->_name);
    }

    SceneEngine::TerrainCoverageId TerrainConfig::CoverageLayerDesc::Id::get()
    {
        return _native->_id;
    }

    VectorUInt2 TerrainConfig::CoverageLayerDesc::NodeDims::get()
    {
        return AsVectorUInt2(_native->_nodeDimensions);
    }

    void TerrainConfig::CoverageLayerDesc::NodeDims::set(VectorUInt2 value)
    {
        _native->_nodeDimensions = AsUInt2(value);
    }

    unsigned TerrainConfig::CoverageLayerDesc::Overlap::get()
    {
        return _native->_overlap;
    }

    void TerrainConfig::CoverageLayerDesc::Overlap::set(unsigned value)
    {
        _native->_overlap = value;
    }

    unsigned TerrainConfig::CoverageLayerDesc::FormatCat::get()
    {
        return _native->_typeCat;
    }
    
    void TerrainConfig::CoverageLayerDesc::FormatCat::set(unsigned value)
    {
        _native->_typeCat = value;
    }

    unsigned TerrainConfig::CoverageLayerDesc::FormatArrayCount::get()
    {
        return _native->_typeCount;
    }
    
    void TerrainConfig::CoverageLayerDesc::FormatArrayCount::set(unsigned value)
    {
        _native->_typeCount = value;
    }

    TerrainConfig::CoverageLayerDesc::CoverageLayerDesc(
        String^ uberSurfaceDirectory, SceneEngine::TerrainCoverageId id)
    {
        ::Assets::ResChar buffer[MaxPath];
        NativeConfig::GetUberSurfaceFilename(
            buffer, dimof(buffer),
            clix::marshalString<clix::E_UTF8>(uberSurfaceDirectory).c_str(), id);
        _native.reset(new NativeConfig::CoverageLayer);
        _native->_name = (const utf8*)buffer;
        _native->_id = id;
        _native->_nodeDimensions = UInt2(0,0);
        _native->_overlap = 0;
        _native->_typeCat = 0;
        _native->_typeCount = 0;
    }

    TerrainConfig::CoverageLayerDesc::CoverageLayerDesc(const NativeConfig::CoverageLayer& native)
    {
        _native.reset(new NativeConfig::CoverageLayer(native));
    }

    TerrainConfig::CoverageLayerDesc::~CoverageLayerDesc() {}

    
///////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class TerrainImportOp
    {
    public:
        property VectorUInt2 SourceDims     { VectorUInt2 get() { return AsVectorUInt2(_native->_sourceDims); } }
        property Vector2 SourceHeightRange  { Vector2 get() { return AsVector2(_native->_sourceHeightRange); } }
        property bool SourceIsGood          { bool get() { return _native->_sourceIsGood; } }

        property VectorUInt2 ImportMins     
        { 
            VectorUInt2 get()           { return AsVectorUInt2(_native->_importMins); }
            void set(VectorUInt2 value) { _native->_importMins = AsUInt2(value); }
        }
        property VectorUInt2 ImportMaxs     
        { 
            VectorUInt2 get()           { return AsVectorUInt2(_native->_importMaxs); }
            void set(VectorUInt2 value) { _native->_importMaxs = AsUInt2(value); }
        }
        property Vector2 ImportHeightRange  
        { 
            Vector2 get()               { return AsVector2(_native->_importHeightRange); }
            void set(Vector2 value)     { _native->_importHeightRange = AsFloat2(value); }
        }

        property bool AbsoluteHeights
        {
            bool get() { return _native->_sourceFormat == ToolsRig::TerrainImportOp::SourceFormat::AbsoluteFloats; }
        }

        property unsigned   WarningCount                { unsigned get() { return (unsigned)_native->_warnings.size(); } }
        String^             Warning(unsigned index)     { return clix::marshalString<clix::E_UTF8>(_native->_warnings[index]); }

        void Execute(String^ outputDir, IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::ExecuteTerrainImport(
                *_native, 
                clix::marshalString<clix::E_UTF8>(outputDir).c_str(),
                _destNodeDims, _destCellTreeDepth, nativeProgress.get());
        }

        TerrainImportOp(String^ input, unsigned destNodeDims, unsigned destCellTreeDepth)
        {
            auto r = ToolsRig::PrepareTerrainImport(
                clix::marshalString<clix::E_UTF8>(input).c_str(),
                destNodeDims, destCellTreeDepth);
            _native.reset(new ToolsRig::TerrainImportOp());
            *_native = r;
            _destNodeDims = destNodeDims;
            _destCellTreeDepth = destCellTreeDepth;
        }

    protected:
        clix::auto_ptr<ToolsRig::TerrainImportOp> _native;
        unsigned _destNodeDims;
        unsigned _destCellTreeDepth;
    };

}