// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

using System::Collections::Generic::Dictionary;
using System::String;
using System::Object;
using namespace System::Runtime::Serialization;

namespace ShaderPatcherLayer
{
    public interface class IPreviewBuilder
    {
	public:
		System::Drawing::Bitmap^ BuildPreviewImage(
			GUILayer::MaterialVisSettings^ visSettings,
			String^ materialNames,
			GUILayer::TechniqueDelegateWrapper^ techniqueDelegate,
			System::Drawing::Size^ size);
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

