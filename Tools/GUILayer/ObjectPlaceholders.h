// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "../../RenderCore/Metal/Forward.h"
#include <memory>

namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace SceneEngine { class IIntersectionTester; }

namespace GUILayer
{
    namespace EditorDynamicInterface { class FlexObjectScene; }

    class ObjectPlaceholders : public std::enable_shared_from_this<ObjectPlaceholders>
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext& threadContext, 
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned techniqueIndex);

        void AddAnnotation(EditorDynamicInterface::ObjectTypeId typeId);

        std::shared_ptr<SceneEngine::IIntersectionTester> CreateIntersectionTester();

        ObjectPlaceholders(std::shared_ptr<EditorDynamicInterface::FlexObjectScene> objects);
        ~ObjectPlaceholders();
    protected:
        std::shared_ptr<EditorDynamicInterface::FlexObjectScene> _objects;

        class Annotation
        {
        public:
            EditorDynamicInterface::ObjectTypeId _typeId;
        };
        std::vector<Annotation> _annotations;

        class IntersectionTester;
    };
}

