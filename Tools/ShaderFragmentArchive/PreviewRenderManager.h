// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace ShaderPatcherLayer
{
    class ManagerPimpl;
    class PreviewBuilderPimpl;
    ref class Document;

    using System::Drawing::Size;

    public enum class PreviewGeometry
    {
        Chart, Plane2D, Box, Sphere, Model
    };
    
    public interface class IPreviewBuilder
    {
    public:
        System::Drawing::Bitmap^ Build(Document^ doc, Size^ size, PreviewGeometry geometry, unsigned targetToVisualize);
    };

    public interface class IManager
    {
        IPreviewBuilder^ CreatePreviewBuilder(System::String^ shaderText);
    };

}

