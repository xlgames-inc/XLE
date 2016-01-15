// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../GUILayer/CLIXAutoPtr.h"

namespace ShaderPatcherLayer
{
    ref class Document;

    public enum class PreviewGeometry
    {
        Chart, Plane2D, Box, Sphere, Model
    };
    
    public interface class IPreviewBuilder
    {
    public:
        System::Drawing::Bitmap^ Build(Document^ doc, System::Drawing::Size^ size, PreviewGeometry geometry, unsigned targetToVisualize);
    };

    public interface class IManager
    {
        IPreviewBuilder^ CreatePreviewBuilder(System::String^ shaderText);
    };

	class AttachPimpl;
	public ref class LibraryAttachMarker
	{
	public:
		LibraryAttachMarker(GUILayer::EngineDevice^ engineDevice);
		~LibraryAttachMarker();
		!LibraryAttachMarker();
	private:
		AttachPimpl* _pimpl;
	};

}

