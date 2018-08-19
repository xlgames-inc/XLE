// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderGenerator.h"
#include "../GUILayer/CLIXAutoPtr.h"

using System::Collections::Generic::Dictionary;
using System::String;
using System::Object;
using namespace System::Runtime::Serialization;

namespace ShaderPatcherLayer
{
	ref class NodeGraphPreviewConfiguration;
    
    public interface class IPreviewBuilder
    {
	public:
		System::Drawing::Bitmap^ BuildPreviewImage(
            NodeGraphMetaData^ doc, 
			NodeGraphPreviewConfiguration^ nodeGraphFile,
			System::Drawing::Size^ size, 
            PreviewGeometry geometry, 
			unsigned targetToVisualize);
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

