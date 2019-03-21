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

namespace GUILayer
{
	ref class MaterialVisSettings;
	ref class TechniqueDelegateWrapper;

    public interface class IPreviewBuilder
    {
	public:
		System::Drawing::Bitmap^ BuildPreviewImage(
			MaterialVisSettings^ visSettings,
			String^ materialNames,
			TechniqueDelegateWrapper^ techniqueDelegate,
			System::Drawing::Size^ size);
    };

	public ref class PreviewBuilderUtils
	{
		static IPreviewBuilder^ CreatePreviewBuilder();
	};
}

