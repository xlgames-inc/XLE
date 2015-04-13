// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Assets/Assets.h"
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
        WorldPlacementsConfig();
    };

    class PlacementsRenderer;
    class PlacementsEditor;
    class PlacementsQuadTree;

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

        auto GetVisibleQuadTrees(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, const PlacementsQuadTree*>>;

        struct ObjectBoundingBoxes { const std::pair<Float3, Float3> * _boundingBox; unsigned _stride; unsigned _count; };
        auto GetObjectBoundingBoxes(const Float4x4& worldToClip) const
            -> std::vector<std::pair<Float3x4, ObjectBoundingBoxes>>;

        std::shared_ptr<PlacementsRenderer> GetRenderer();
        std::shared_ptr<PlacementsEditor> CreateEditor();

        PlacementsManager(
            const WorldPlacementsConfig& cfg,
            std::shared_ptr<RenderCore::Assets::IModelFormat> modelFormat,
            const Float2& worldOffset);
        ~PlacementsManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class PlacementsEditor;
    };

    typedef std::pair<uint64, uint64> PlacementGUID;
    class PlacementCell;

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

        void RenderFiltered(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext,
            unsigned techniqueIndex,
            const PlacementGUID* begin, const PlacementGUID* end);

        void RegisterCell(
            const PlacementCell& cell,
            const Float2& mins, const Float2& maxs);

        uint64 CreateCell(
            PlacementsManager& manager,
            const ::Assets::ResChar name[],
            const Float2& mins, const Float2& maxs);
        void RemoveCell(PlacementsManager& manager, uint64 id);
        static uint64 GenerateObjectGUID();
		void PerformGUIDFixup(PlacementGUID* begin, PlacementGUID* end) const;

        void Save();

        std::shared_ptr<RenderCore::Assets::IModelFormat> GetModelFormat();
        std::pair<Float3, Float3> GetModelBoundingBox(const Assets::ResChar modelName[]) const;

        PlacementsEditor(std::shared_ptr<PlacementsRenderer> renderer);
        ~PlacementsEditor();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class Transaction;
    };
}

