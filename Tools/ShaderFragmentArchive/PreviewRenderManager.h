// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace ShaderDiagram { ref class Document; }

namespace PreviewRender
{
    class ManagerPimpl;
    class PreviewBuilderPimpl;

    using System::Drawing::Size;

    public enum class PreviewGeometry
    {
        Chart, Box, Sphere, Model
    };
    
    public interface class IPreviewBuilder
    {
    public:
        System::Drawing::Bitmap^ Build(
            ShaderDiagram::Document^ doc, Size^ size, PreviewGeometry geometry);
    };

    public interface class IManager
    {
        IPreviewBuilder^ CreatePreviewBuilder(System::String^ shaderText);
    };

}

