// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include <memory>

namespace Assets { class DependencyValidation; }
namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class DualContourMesh;

    class DualContourRenderer
    {
    public:
        void Render(
            RenderCore::Metal::DeviceContext* context, 
            RenderCore::Techniques::ParsingContext& lightingParserContext,
            unsigned techniqueIndex) const;

        void RenderAsCloud( 
            RenderCore::Metal::DeviceContext* context, 
            RenderCore::Techniques::ParsingContext& lightingParserContext);

        void RenderUnsortedTrans(
            RenderCore::Metal::DeviceContext* context, 
            RenderCore::Techniques::ParsingContext& lightingParserContext,
            unsigned techniqueIndex) const;

        DualContourRenderer(const DualContourMesh& mesh);
        ~DualContourRenderer();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation();

        class Pimpl;
    private:
        std::unique_ptr<Pimpl> _pimpl;
    };

    void    DualContourMesh_DebuggingRender(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parsingContext,
        unsigned techniqueIndex, const DualContourMesh& mesh);

}