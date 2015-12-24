// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/ShaderService.h" // for RenderCore::ShaderService::IShaderSource
#include "../../RenderCore/Techniques/Techniques.h"
#include "ShaderDiagramDocument.h"
#include <memory>

namespace PreviewRender
{
    class ManagerPimpl;
    class PreviewBuilderPimpl;

    using System::Drawing::Size;

    public ref class PreviewBuilder
    {
    public:
        property System::Drawing::Bitmap^   Bitmap
        {
            System::Drawing::Bitmap^ get() { return _bitmap; }
        }

        enum class PreviewGeometry
        {
            Chart, Box, Sphere, Model
        };

        void    Update(ShaderDiagram::Document^ doc, Size^ size, PreviewGeometry geometry);
        void    Invalidate();

        PreviewBuilder(
            std::shared_ptr<RenderCore::ShaderService::IShaderSource> shaderSource, 
            System::String^ shaderText);
        ~PreviewBuilder();
    private:
        PreviewBuilderPimpl*        _pimpl;
        System::Drawing::Bitmap^    _bitmap;

        System::Drawing::Bitmap^    GenerateBitmap(ShaderDiagram::Document^ doc, Size^ size, PreviewGeometry geometry);
        System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
    };

    public ref class Manager
    {
    public:
        PreviewBuilder^         CreatePreview(System::String^ shaderText);
        void                    RotateLightDirection(ShaderDiagram::Document^ doc, System::Drawing::PointF rotationAmount);
        RenderCore::IDevice*    GetDevice();
        auto                    GetGlobalTechniqueContext() -> RenderCore::Techniques::TechniqueContext*;
        void                    Update();

        static property Manager^     Instance
        {
            Manager^ get() { if (!_instance) { _instance = gcnew Manager(); } return _instance; }
        }

        static void Shutdown();

    private:
        ManagerPimpl*       _pimpl;
        static Manager^     _instance;

        Manager();
        ~Manager();
        static Manager();
    };

}

