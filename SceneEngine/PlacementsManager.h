// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Core/Types.h"
#include <string>
#include <functional>

namespace RenderCore { namespace Assets { class IModelFormat; } }

namespace SceneEngine
{
    class LightingParserContext;

    class WorldPlacementsConfig
    {
    public:
        std::string _baseDir;
        float _cellSize;
        UInt2 _cellCount;

        WorldPlacementsConfig(const std::string& baseDir);
    };

    class PlacementsRenderer;
    class PlacementsEditor;

    /// <summmary>Manages stream and organization of object placements</summary>
    /// In this context, placements are static objects placed in the world. Most
    /// scenes will have a large number of essentially static objects. This object
    /// manages a large continuous world of these kinds of objects.
    class PlacementsManager
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext,
            unsigned techniqueIndex);

        std::shared_ptr<PlacementsRenderer> GetRenderer();
        std::shared_ptr<PlacementsEditor> CreateEditor();

        PlacementsManager(
            const WorldPlacementsConfig& cfg,
            std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat);
        ~PlacementsManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    typedef std::pair<uint64, uint64> PlacementGUID;

    class PlacementsEditor
    {
    public:
        typedef Float3x4 PlacementsTransform;

        PlacementGUID AddPlacement(
            const PlacementsTransform& objectToWorld, 
            const char modelFilename[], const char materialFilename[]);

        class ObjectDef
        {
        public:
            Float4x4 _localToWorld;
            std::pair<Float3, Float3> _localSpaceBoundingBox;
            uint64 _model;
            uint64 _material;
        };

        std::vector<PlacementGUID> FindPlacements(
            const Float3& worldSpaceMins, const Float3& worldSpaceMaxs,
            const std::function<bool(const ObjectDef&)>& predicate = nullptr);

        void DeletePlacements(const std::vector<PlacementGUID>& placements);

        void RegisterCell(  
            const Float2& mins, const Float2& maxs, 
            const Float4x4& cellToWorld,
            const char name[], uint64 guid);

        std::shared_ptr<RenderCore::Assets::IModelFormat> GetModelFormat();

        PlacementsEditor(std::shared_ptr<PlacementsRenderer> renderer);
        ~PlacementsEditor();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

