// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Assets/Assets.h"
#include "../Utility/UTFUtils.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Core/Types.h"
#include <string>
#include <functional>

namespace RenderCore { namespace Assets { class ModelCache; class DelayedDrawCall; enum class DelayStep : unsigned; } }
namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace Utility { class OutputStream; template<typename CharType> class InputStreamFormatter; }
namespace Assets { class DirectorySearchRules; }

namespace SceneEngine
{
    class WorldPlacementsConfig
    {
    public:
        class Cell
        {
        public:
            Float3 _offset;
            Float3 _mins, _maxs;
            ::Assets::ResChar _file[MaxPath];
        };
        std::vector<Cell> _cells;

        WorldPlacementsConfig(
            Utility::InputStreamFormatter<utf8>& formatter,
            const ::Assets::DirectorySearchRules& searchRules);
        WorldPlacementsConfig();
    };

    class PlacementsRenderer;
    class PlacementsIntersections;
    class PlacementsEditor;
    class PlacementsQuadTree;
    class DynamicImposters;

    /// <summary>A collection of cells</summary>
    /// 
    class PlacementCellSet
    {
    public:
        PlacementCellSet(const WorldPlacementsConfig& cfg, const Float3& worldOffset);
        ~PlacementCellSet();

        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    /// <summary>Manages stream and organization of object placements</summary>
    /// In this context, placements are static objects placed in the world. Most
    /// scenes will have a large number of essentially static objects. This object
    /// manages a large continuous world of these kinds of objects.
    class PlacementsManager
    {
    public:
        std::shared_ptr<PlacementsRenderer>         GetRenderer();
        std::shared_ptr<PlacementsEditor>           CreateEditor(const std::shared_ptr<PlacementCellSet>& cellSet);
        std::shared_ptr<PlacementsIntersections>    GetIntersections();

        PlacementsManager(std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~PlacementsManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class PlacementsEditor;
    };
    
    class PlacementCell;
    class PlacementsCache;
    typedef std::pair<uint64, uint64> PlacementGUID;
    
    class PlacementsRenderer
    {
    public:
            // -------------- Rendering --------------
        void Render(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const PlacementCellSet& cellSet);
        void CommitTransparent(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex, RenderCore::Assets::DelayStep delayStep);
        bool HasPrepared(RenderCore::Assets::DelayStep delayStep);

            // -------------- Render filtered --------------
        using DrawCallPredicate = std::function<bool(const RenderCore::Assets::DelayedDrawCall&)>;
        void RenderFiltered(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const PlacementCellSet& cellSet,
            const PlacementGUID* begin, const PlacementGUID* end,
            const DrawCallPredicate& predicate = DrawCallPredicate(nullptr));

            // -------------- Utilities --------------
        auto GetVisibleQuadTrees(const PlacementCellSet& cellSet, const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, const PlacementsQuadTree*>>;

        struct ObjectBoundingBoxes { const std::pair<Float3, Float3> * _boundingBox; unsigned _stride; unsigned _count; };
        auto GetObjectBoundingBoxes(const PlacementCellSet& cellSet, const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, ObjectBoundingBoxes>>;

        void SetImposters(std::shared_ptr<DynamicImposters> imposters);

        PlacementsRenderer(
            std::shared_ptr<PlacementsCache> placementsCache, 
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~PlacementsRenderer();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class PlacementsIntersections
    {
    public:
                // -------------- Intersections --------------
        class IntersectionDef
        {
        public:
            Float3x4    _localToWorld;
            std::pair<Float3, Float3> _localSpaceBoundingBox;
            uint64      _model;
            uint64      _material;
        };

        std::vector<PlacementGUID> Find_BoxIntersection(
            const PlacementCellSet& cellSet,
            const Float3& worldSpaceMins, const Float3& worldSpaceMaxs,
            const std::function<bool(const IntersectionDef&)>& predicate);

        std::vector<PlacementGUID> Find_RayIntersection(
            const PlacementCellSet& cellSet,
            const Float3& rayStart, const Float3& rayEnd,
            const std::function<bool(const IntersectionDef&)>& predicate);

        std::vector<PlacementGUID> Find_FrustumIntersection(
            const PlacementCellSet& cellSet,
            const Float4x4& worldToProjection,
            const std::function<bool(const IntersectionDef&)>& predicate);

        PlacementsIntersections(
            std::shared_ptr<PlacementsCache> placementsCache, 
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~PlacementsIntersections();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class PlacementsEditor
    {
    public:
        typedef Float3x4 PlacementsTransform;

        class ObjTransDef
        {
        public:
            Float3x4        _localToWorld;
            std::string     _model;
            std::string     _material;
            std::string     _supplements;

            ObjTransDef() {}
            ObjTransDef(const Float3x4& localToWorld, const std::string& model, const std::string& material, const std::string& supplements)
                : _localToWorld(localToWorld), _model(model), _material(material), _supplements(supplements), _transaction(Error) {}

            enum TransactionType { Unchanged, Created, Deleted, Modified, Error };
            TransactionType _transaction;
        };

            // -------------- transactions --------------
        class ITransaction
        {
        public:
            virtual const ObjTransDef&  GetObject(unsigned index) const = 0;
            virtual const ObjTransDef&  GetObjectOriginalState(unsigned index) const = 0;
            virtual PlacementGUID       GetGuid(unsigned index) const = 0;
            virtual PlacementGUID       GetOriginalGuid(unsigned index) const = 0;
            virtual unsigned            GetObjectCount() const = 0;
            virtual auto                GetLocalBoundingBox(unsigned index) const -> std::pair<Float3, Float3> = 0;
            virtual auto                GetWorldBoundingBox(unsigned index) const -> std::pair<Float3, Float3> = 0;
            virtual std::string         GetMaterialName(unsigned objectIndex, uint64 materialGuid) const = 0;

            virtual void    SetObject(unsigned index, const ObjTransDef& newState) = 0;
            virtual bool    Create(const ObjTransDef& newState) = 0;
            virtual bool    Create(PlacementGUID guid, const ObjTransDef& newState) = 0;
            virtual void    Delete(unsigned index) = 0;

            virtual void    Commit() = 0;
            virtual void    Cancel() = 0;
            virtual void    UndoAndRestart() = 0;
        };

        struct TransactionFlags { 
            enum Flags { IgnoreIdTop32Bits = 1<<1 };
            typedef unsigned BitField;
        };
        std::shared_ptr<ITransaction> Transaction_Begin(
            const PlacementGUID* placementsBegin, const PlacementGUID* placementsEnd,
            TransactionFlags::BitField transactionFlags = 0);

        uint64  CreateCell(const ::Assets::ResChar name[], const Float2& mins, const Float2& maxs);
        bool    RemoveCell(uint64 id);
        static uint64 GenerateObjectGUID();
        void    PerformGUIDFixup(PlacementGUID* begin, PlacementGUID* end) const;

        std::pair<Float3, Float3> CalculateCellBoundary(uint64 cellId) const;

        std::string GetMetricsString(uint64 cellId) const;
        void WriteAllCells();
        void WriteCell(uint64 cellId, const Assets::ResChar destinationFile[]) const;

        std::pair<Float3, Float3> GetModelBoundingBox(const Assets::ResChar modelName[]) const;

        PlacementsEditor(
            std::shared_ptr<PlacementCellSet> cellSet,
            std::shared_ptr<PlacementsCache> placementsCache, 
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache);
        ~PlacementsEditor();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class Transaction;
    };

}

