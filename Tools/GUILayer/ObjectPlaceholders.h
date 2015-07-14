// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../EntityInterface/EntityInterface.h"
#include "../../RenderCore/Metal/Forward.h"
#include <memory>
#include <string>

namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace SceneEngine { class IIntersectionTester; }

namespace EntityInterface { class RetainedEntities; }

namespace GUILayer
{
    class ObjectPlaceholders : public std::enable_shared_from_this<ObjectPlaceholders>
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext& threadContext, 
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);

        void AddAnnotation(EntityInterface::ObjectTypeId typeId, const std::string& geoType);

        std::shared_ptr<SceneEngine::IIntersectionTester> CreateIntersectionTester();

        ObjectPlaceholders(std::shared_ptr<EntityInterface::RetainedEntities> objects);
        ~ObjectPlaceholders();
    protected:
        std::shared_ptr<EntityInterface::RetainedEntities> _objects;

        class Annotation
        {
        public:
            EntityInterface::ObjectTypeId _typeId;
        };
        std::vector<Annotation> _cubeAnnotations;
        std::vector<Annotation> _triMeshAnnotations;

        class IntersectionTester;
    };
}

