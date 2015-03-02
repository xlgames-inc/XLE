// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IDevice_Forward.h"
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

        void    Update(ShaderDiagram::Document^ doc, Size^ size);
        void    Invalidate();

        static void Shutdown();

        PreviewBuilder(System::String^ shaderText);
        ~PreviewBuilder();
    private:
        PreviewBuilderPimpl*        _pimpl;
        System::Drawing::Bitmap^    _bitmap;

        System::Drawing::Bitmap^    GenerateBitmap(ShaderDiagram::Document^ doc, Size^ size);
        System::Drawing::Bitmap^    GenerateErrorBitmap(const char str[], Size^ size);
    };

    public ref class Manager
    {
    public:
        PreviewBuilder^         CreatePreview(System::String^ shaderText);
        void                    RotateLightDirection(ShaderDiagram::Document^ doc, System::Drawing::PointF rotationAmount);
        RenderCore::IDevice*    GetDevice();

        static property Manager^     Instance
        {
            Manager^ get() { if (!_instance) { _instance = gcnew Manager(); } return _instance; }
        }

    private:
        ManagerPimpl*       _pimpl;
        static Manager^     _instance;

        Manager();
        ~Manager();
        static Manager();
    };

}

