// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderGenerator.h"
#include "../GUILayer/CLIXAutoPtr.h"
#include <memory>

using System::Collections::Generic::Dictionary;
using System::String;
using System::Object;
using namespace System::Runtime::Serialization;

namespace ToolsRig { class DelegateActualizationMessages; }

namespace ShaderPatcherLayer
{
	ref class NodeGraphPreviewConfiguration;
    
	public ref class DelegateActualizationMessagesWrapper
	{
	public:
		System::Collections::Generic::IEnumerable<System::String^>^ GetMessages();

		delegate void OnChangeEventHandler(System::Object^ sender, System::EventArgs^ args);
		property OnChangeEventHandler^ OnChangeEvent;

		clix::shared_ptr<ToolsRig::DelegateActualizationMessages> _native;
		unsigned _callbackId;

		DelegateActualizationMessagesWrapper(const std::shared_ptr<ToolsRig::DelegateActualizationMessages>& techniqueDelegate);
		DelegateActualizationMessagesWrapper(ToolsRig::DelegateActualizationMessages* techniqueDelegate);
		DelegateActualizationMessagesWrapper();
        ~DelegateActualizationMessagesWrapper();
	};

    public interface class IPreviewBuilder
    {
	public:
		System::Drawing::Bitmap^ BuildPreviewImage(
            NodeGraphMetaData^ doc, 
			NodeGraphPreviewConfiguration^ nodeGraphFile,
			System::Drawing::Size^ size, 
            PreviewGeometry geometry, 
			unsigned targetToVisualize);

		GUILayer::TechniqueDelegateWrapper^ MakeTechniqueDelegate(
			NodeGraphMetaData^ doc, 
			NodeGraphPreviewConfiguration^ nodeGraphFile);

		GUILayer::TechniqueDelegateWrapper^ MakeTechniqueDelegate(
			NodeGraphFile^ nodeGraph,
			String^ subGraphName,
			DelegateActualizationMessagesWrapper^ logMessages);
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

