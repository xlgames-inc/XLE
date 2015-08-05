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

namespace RenderCore { namespace Assets { class ModelCache; class DelayedDrawCall; } }
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
    class PlacementsEditor;
    class PlacementsQuadTree;

    /// <summary>Manages stream and organization of object placements</summary>
    /// In this context, placements are static objects placed in the world. Most
    /// scenes will have a large number of essentially static objects. This object
    /// manages a large continuous world of these kinds of objects.
    class PlacementsManager
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);
        void RenderTransparent(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);

        auto GetVisibleQuadTrees(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, const PlacementsQuadTree*>>;

        struct ObjectBoundingBoxes { const std::pair<Float3, Float3> * _boundingBox; unsigned _stride; unsigned _count; };
        auto GetObjectBoundingBoxes(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, ObjectBoundingBoxes>>;

        std::shared_ptr<PlacementsRenderer> GetRenderer();
        std::shared_ptr<PlacementsEditor> CreateEditor();

        PlacementsManager(
            const WorldPlacementsConfig& cfg,
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache,
            const Float3& worldOffset);
        ~PlacementsManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class PlacementsEditor;
    };

    typedef std::pair<uint64, uint64> PlacementGUID;
    class PlacementCell;
    class PlacementsCache;

    class PlacementsEditor
    {
    public:
        typedef Float3x4 PlacementsTransform;

        class ObjIntersectionDef
        {
        public:
            Float3x4 _localToWorld;
            std::pair<Float3, Float3> _localSpaceBoundingBox;
            uint64 _model;
            uint64 _material;
        };

        class ObjTransDef
        {
        public:
            Float3x4        _localToWorld;
            std::string     _model;
            std::string     _material;

            ObjTransDef() {}
            ObjTransDef(const Float3x4& localToWorld, const std::string& model, const std::string& material)
                : _localToWorld(localToWorld), _model(model), _material(material), _transaction(Error) {}

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

            // -------------- intersections --------------
        std::vector<PlacementGUID> Find_BoxIntersection(
            const Float3& worldSpaceMins, const Float3& worldSpaceMaxs,
            const std::function<bool(const ObjIntersectionDef&)>& predicate);

        std::vector<PlacementGUID> Find_RayIntersection(
            const Float3& rayStart, const Float3& rayEnd,
            const std::function<bool(const ObjIntersectionDef&)>& predicate);

        std::vector<PlacementGUID> Find_FrustumIntersection(
            const Float4x4& worldToProjection,
            const std::function<bool(const ObjIntersectionDef&)>& predicate);

        using DrawCallPredicate = std::function<bool(const RenderCore::Assets::DelayedDrawCall&)>;

        void RenderFiltered(
            RenderCore::Metal::DeviceContext* context,
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex,
            const PlacementGUID* begin, const PlacementGUID* end,
            const DrawCallPredicate& predicate = DrawCallPredicate(nullptr));

        void RegisterCell(
            const PlacementCell& cell,
            const Float2& mins, const Float2& maxs);

        uint64 CreateCell(
            PlacementsManager& manager,
            const ::Assets::ResChar name[],
            const Float2& mins, const Float2& maxs);
        bool RemoveCell(PlacementsManager& manager, uint64 id);
        static uint64 GenerateObjectGUID();
		void PerformGUIDFixup(PlacementGUID* begin, PlacementGUID* end) const;

        std::pair<Float3, Float3> CalculateCellBoundary(uint64 cellId) const;

        std::string GetMetricsString(uint64 cellId) const;
        void WriteAllCells();
        void WriteCell(uint64 cellId, const Assets::ResChar destinationFile[]) const;

        std::pair<Float3, Float3> GetModelBoundingBox(const Assets::ResChar modelName[]) const;

        PlacementsEditor(
            std::shared_ptr<PlacementsCache> placementsCache, 
            std::shared_ptr<RenderCore::Assets::ModelCache> modelCache,
            std::shared_ptr<PlacementsRenderer> renderer);
        ~PlacementsEditor();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class Transaction;
    };
}

