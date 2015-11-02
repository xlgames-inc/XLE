// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainManipulatorsCommon.h"
#include "ManipulatorsUtil.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/Font.h"

#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/IntersectionTest.h"

#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"

#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/RenderUtils.h"

#include "../../Math/ProjectionMath.h"
#include "../../Math/Transformations.h"
#include "../../Utility/TimeUtils.h"
#include "../../ConsoleRig/Console.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

extern unsigned FrameRenderCount;

namespace ToolsRig
{
    using SceneEngine::IntersectionTestContext;
    using SceneEngine::IntersectionTestScene;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //      M A N I P U L A T O R S             //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RaiseLowerManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Raise and Lower"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }

        RaiseLowerManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _powerValue;
    };

    void    RaiseLowerManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength)
    {
            //
            //      Use the uber surface interface to change these values
            //          -- this will make sure all of the cells get updated as needed
            //
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
            i->AdjustHeights(
                context,
                WorldSpaceToTerrain(Truncate(worldSpacePosition)), 
                WorldSpaceDistanceToTerrainCoords(size), 
                .05f * strength, _powerValue);
        }
    }

    auto RaiseLowerManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&RaiseLowerManipulator::_powerValue), 1.f/8.f, 8.f, FloatParameter::Linear, "ShapeControl")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    RaiseLowerManipulator::RaiseLowerManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _powerValue = 1.f/8.f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class SmoothManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Smooth"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }

        SmoothManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _standardDeviation;
        float _filterRadius;
        unsigned _flags;
    };

    void    SmoothManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
            i->Smooth(
                context,
                WorldSpaceToTerrain(Truncate(worldSpacePosition)), 
                WorldSpaceDistanceToTerrainCoords(size),
                unsigned(_filterRadius), 
                _standardDeviation, _strength, _flags);
        }
    }

    auto SmoothManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_standardDeviation), 1.f, 6.f, FloatParameter::Linear, "Blurriness"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_filterRadius), 2.f, 16.f, FloatParameter::Linear, "FilterRadius"),
            FloatParameter(ManipulatorParameterOffset(&SmoothManipulator::_strength), 0.01f, 1.f, FloatParameter::Linear, "Strength")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto SmoothManipulator::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&SmoothManipulator::_flags), 0, "SmoothUp"),
            BoolParameter(ManipulatorParameterOffset(&SmoothManipulator::_flags), 1, "SmoothDown")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    SmoothManipulator::SmoothManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _standardDeviation = 3.f;
        _filterRadius = 16.f;
        _flags = 0x3;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class NoiseManipulator : public CommonManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Add Noise"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }

        NoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    };

    void    NoiseManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
            i->AddNoise(
                context,
                WorldSpaceToTerrain(Truncate(worldSpacePosition)), 
                WorldSpaceDistanceToTerrainCoords(size), 
                .05f * strength);
        }
    }

    auto NoiseManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&NoiseManipulator::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&NoiseManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    NoiseManipulator::NoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {}

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class CopyHeight : public CommonManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Copy Height"; }

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }

        CopyHeight(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
    private:
        float _powerValue;
        unsigned _flags;
    };

    void CopyHeight::PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i && _targetOnMouseDown.second) {
            i->CopyHeight(
                context,
                WorldSpaceToTerrain(Truncate(worldSpacePosition)), 
                WorldSpaceToTerrain(Truncate(_targetOnMouseDown.first)), 
                WorldSpaceDistanceToTerrainCoords(size), 
                strength, _powerValue, _flags);
        }
    }

    auto CopyHeight::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_strength), 1.f, 100.f, FloatParameter::Logarithmic, "Strength"),
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_size), 0.1f, 500.f, FloatParameter::Linear, "Size"),
            FloatParameter(ManipulatorParameterOffset(&CopyHeight::_powerValue), 1.f/8.f, 8.f, FloatParameter::Linear, "ShapeControl")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto CopyHeight::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&CopyHeight::_flags), 0, "MoveUp"),
            BoolParameter(ManipulatorParameterOffset(&CopyHeight::_flags), 1, "MoveDown")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    CopyHeight::CopyHeight(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(std::move(terrainManager))
    {
        _powerValue = 1.f/8.f;
        _flags = 0x3;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class FillNoiseManipulator : public RectangleManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& anchor0, const Float3& anchor1);
        virtual const char* GetName() const { return "Fill noise"; }
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }

        FillNoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        float _baseHeight, _noiseHeight, _roughness, _fractalDetail;
    };

    void    FillNoiseManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& anchor0, const Float3& anchor1)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
            i->FillWithNoise(
                context,
                WorldSpaceToTerrain(Truncate(anchor0)), 
                WorldSpaceToTerrain(Truncate(anchor1)), 
                _baseHeight, _noiseHeight, _roughness, _fractalDetail);
        }
    }

    auto FillNoiseManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_baseHeight), 0.1f, 1000.f, FloatParameter::Logarithmic, "BaseHeight"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_noiseHeight), 0.1f, 2000.f, FloatParameter::Logarithmic, "NoiseHeight"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_roughness), 10.f, 1000.f, FloatParameter::Logarithmic, "Roughness"),
            FloatParameter(ManipulatorParameterOffset(&FillNoiseManipulator::_fractalDetail), 0.1f, 0.9f, FloatParameter::Linear, "FractalDetail")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    FillNoiseManipulator::FillNoiseManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _baseHeight = 250.0f;
        _noiseHeight = 500.f;
        _roughness = 250.f;
        _fractalDetail = 0.5f;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class PaintCoverageManipulator : public CommonManipulator
    {
    public:
        virtual void PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength);
        virtual const char* GetName() const { return "Paint Coverage"; }
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const;
        virtual void SetActivationState(bool newState);

        PaintCoverageManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        unsigned _coverageLayer;
        unsigned _paintValue;
    };

    // unsigned FindLayerIndex(const SceneEngine::TerrainConfig& cfg, SceneEngine::TerrainCoverageId layerId)
    // {
    //     for (unsigned c = 0; c<cfg.GetCoverageLayerCount(); ++c)
    //         if (cfg.GetCoverageLayer(c)._id == layerId) return c;
    //     return ~unsigned(0x0);
    // }

    void PaintCoverageManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& worldSpacePosition, float size, float strength)
    {
        auto* i = _terrainManager->GetCoverageInterface(_coverageLayer);
        if (i) {
            i->Paint(
                context,
                WorldSpaceToCoverage(_coverageLayer, Truncate(worldSpacePosition)), 
                WorldSpaceToCoverageDistance(_coverageLayer, size),
                _paintValue);
        }
    }

    auto PaintCoverageManipulator::GetIntParameters() const -> std::pair < IntParameter*, size_t >
    {
        static IntParameter parameters[] = 
        {
            IntParameter(ManipulatorParameterOffset(&PaintCoverageManipulator::_coverageLayer), 0i32, INT32_MAX, IntParameter::Linear, "CoverageLayer"),
            IntParameter(ManipulatorParameterOffset(&PaintCoverageManipulator::_paintValue), 0i32, INT32_MAX, IntParameter::Linear, "PaintValue"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto PaintCoverageManipulator::GetFloatParameters() const -> std::pair < FloatParameter*, size_t >
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&PaintCoverageManipulator::_size), 0.1f, 500.f, FloatParameter::Linear, "Size")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    void PaintCoverageManipulator::SetActivationState(bool newState)
    {
        Tweakable("TerrainVisCoverage", 0) = newState ? _coverageLayer : 0;
    }

    PaintCoverageManipulator::PaintCoverageManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : CommonManipulator(terrainManager)
    {
        _coverageLayer = 1000;
        _paintValue = 1;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class ErosionManipulator : public RectangleManipulator
    {
    public:
        virtual void            PerformAction(RenderCore::IThreadContext& context, const Float3& anchor0, const Float3& anchor1);
        virtual const char*     GetName() const { return "Erosion simulation"; }
        virtual void            Render(RenderCore::IThreadContext& context, SceneEngine::LightingParserContext& parserContext);

        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }
        
        ErosionManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        Float2  _activeMins;
        Float2  _activeMaxs;
        SceneEngine::ErosionSimulation::Settings _params;
        unsigned _flags;

        size_t OffsetOf(const void* member) const;
    };

    void    ErosionManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3& anchor0, const Float3& anchor1)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
            _activeMins = WorldSpaceToTerrain(Truncate(anchor0));
            _activeMaxs = WorldSpaceToTerrain(Truncate(anchor1));
            _flags = _flags & ~(1<<0);    // start inactive
            i->Erosion_End();
        }
    }

    void    ErosionManipulator::Render(RenderCore::IThreadContext& context, SceneEngine::LightingParserContext& parserContext)
    {
        RectangleManipulator::Render(context, parserContext);

            //  Doing the erosion tick here is most convenient (because this is the 
            //  only place we get regular updates).
        if (_flags & (1<<0)) {
            auto *i = _terrainManager->GetHeightsInterface();
            if (i) {
                if (_activeMaxs[0] > _activeMins[0] && _activeMaxs[1] > _activeMins[1]) {
                    if (!i->Erosion_IsPrepared()) {
                        i->Erosion_Begin(context, _activeMins, _activeMaxs, _terrainManager->GetConfig());
                    }

                    i->Erosion_Tick(context, _params);
                }
            }
        }

        if (_flags & (1<<1)) {
            auto *i = _terrainManager->GetHeightsInterface();
            if (i && i->Erosion_IsPrepared()) {
                i->Erosion_RenderDebugging(context, parserContext, _terrainManager->GetCoords());
            }
        }
    }

    size_t ErosionManipulator::OffsetOf(const void* member) const
    {
        auto offset = size_t(member) - size_t((const IManipulator*)this);
        assert(offset < sizeof(*this));
        return offset;
    }

    auto ErosionManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(OffsetOf(&_params._rainQuantityPerFrame), 0.f, 1.f, FloatParameter::Linear, "Rain quantity per frame"),
            FloatParameter(OffsetOf(&_params._evaporationConstant), 0.f, 1.f, FloatParameter::Linear, "Evaporation constant"),
            FloatParameter(OffsetOf(&_params._pressureConstant), 0.f, 1.f, FloatParameter::Linear, "Pressure constant"),

            FloatParameter(OffsetOf(&_params._kConstant), 0.f, 1.f, FloatParameter::Linear, "K constant"),
            FloatParameter(OffsetOf(&_params._erosionRate), 0.f, 1.f, FloatParameter::Linear, "Erosion rate"),
            FloatParameter(OffsetOf(&_params._settlingRate), 0.f, 1.f, FloatParameter::Linear, "Settling rate"),
            FloatParameter(OffsetOf(&_params._maxSediment), 0.f, 1.f, FloatParameter::Linear, "Max sediment"),
            FloatParameter(OffsetOf(&_params._depthMax), 0.f, 1.f, FloatParameter::Linear, "Depth max"),
            FloatParameter(OffsetOf(&_params._sedimentShiftScalar), 0.f, 1.f, FloatParameter::Linear, "Sediment shift scalar"),

            FloatParameter(OffsetOf(&_params._thermalSlopeAngle), 0.f, 1.f, FloatParameter::Linear, "Thermal slope angle"),
            FloatParameter(OffsetOf(&_params._thermalErosionRate), 0.f, 1.f, FloatParameter::Linear, "Thermal erosion rate")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto ErosionManipulator::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&ErosionManipulator::_flags), 0, "Active"),
            BoolParameter(ManipulatorParameterOffset(&ErosionManipulator::_flags), 1, "Draw water"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    ErosionManipulator::ErosionManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _flags = 0;
        _activeMins = _activeMaxs = Float2(0.f, 0.f);
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RotateManipulator : public RectangleManipulator
    {
    public:
        virtual void    PerformAction(RenderCore::IThreadContext& context, const Float3& anchor0, const Float3& anchor1);
        virtual const char* GetName() const { return "Rotate"; }
        virtual std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        virtual std::pair<BoolParameter*, size_t>   GetBoolParameters() const { return std::make_pair(nullptr, 0); }
        virtual std::pair<IntParameter*, size_t>   GetIntParameters() const { return std::make_pair(nullptr, 0); }
        RotateManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);

    private:
        float _rotationDegrees;
    };

    void    RotateManipulator::PerformAction(RenderCore::IThreadContext& context, const Float3&, const Float3&)
    {
        auto *i = _terrainManager->GetHeightsInterface();
        if (i) {
                // we can't use the parameters passed into this function (because they've
                //  been adjusted to the mins/maxs of a rectangular area, and we've lost
                //  directional information)
            Float2 rotationOrigin = RoundDownToInteger(WorldSpaceToTerrain(Truncate(_firstAnchor)));
            Float2 farPoint = RoundDownToInteger(WorldSpaceToTerrain(Truncate(_secondAnchor.first)));

            float radius = Magnitude(rotationOrigin - farPoint);
            Float2 A(farPoint[0] - rotationOrigin[0], farPoint[1] - rotationOrigin[1]);
            Float3 rotationAxis = Normalize(Float3(A[1], -A[0], 0.f));
            i->Rotate(context, rotationOrigin, radius, rotationAxis, float(_rotationDegrees * M_PI / 180.f));
        }
    }

    auto RotateManipulator::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&RotateManipulator::_rotationDegrees), 1.0f, 70.f, FloatParameter::Logarithmic, "RotationDegrees")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    RotateManipulator::RotateManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : RectangleManipulator(terrainManager)
    {
        _rotationDegrees = 10.f;
    }

    std::vector<std::unique_ptr<IManipulator>> CreateTerrainManipulators(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
    {
        std::vector<std::unique_ptr<IManipulator>> result;
        result.emplace_back(std::make_unique<RaiseLowerManipulator>(terrainManager));
        result.emplace_back(std::make_unique<SmoothManipulator>(terrainManager));
        result.emplace_back(std::make_unique<NoiseManipulator>(terrainManager));
        result.emplace_back(std::make_unique<FillNoiseManipulator>(terrainManager));
        result.emplace_back(std::make_unique<CopyHeight>(terrainManager));
        result.emplace_back(std::make_unique<RotateManipulator>(terrainManager));
        result.emplace_back(std::make_unique<ErosionManipulator>(terrainManager));
        result.emplace_back(std::make_unique<PaintCoverageManipulator>(terrainManager));
        return result;
    }
}

