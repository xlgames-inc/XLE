// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UITypesBinding.h"
#include "EngineDevice.h"

namespace GUILayer 
{
    class VisLayerControllerPimpl;

    ref class ModelVisSettings;
    ref class VisMouseOver;
	ref class VisOverlaySettings;
	ref class LayerControl;
	ref class VisAnimationState;
	ref class RawMaterial;
	ref class TechniqueDelegateWrapper;
	ref class CompiledShaderPatchCollectionWrapper;

	public ref class VisLayerController : public IOnEngineShutdown
    {
	public:
		property VisMouseOver^ MouseOver { VisMouseOver^ get(); }
		property VisAnimationState^ AnimationState { VisAnimationState^ get(); }

		void SetModelSettings(ModelVisSettings^ settings);
		ModelVisSettings^ GetModelSettings();

		void SetMaterialVisSettings(MaterialVisSettings^ settings);
		MaterialVisSettings^ GetMaterialVisSettings();

		void SetOverlaySettings(VisOverlaySettings^ settings);
		VisOverlaySettings^ GetOverlaySettings();

		void SetMaterialOverrides(System::Collections::Generic::IEnumerable<RawMaterial^>^ materialOverrides);
		void SetTechniqueOverrides(TechniqueDelegateWrapper^ techniqueDelegate);
		void SetPatchCollectionOverrides(CompiledShaderPatchCollectionWrapper^ patchCollection);

		void ResetCamera();

		void AttachToView(LayerControl^ view);
		void DetachFromView(LayerControl^ view);

		VisLayerController();
		~VisLayerController();
		!VisLayerController();
		virtual void OnEngineShutdown();

	private:
		clix::auto_ptr<VisLayerControllerPimpl> _pimpl;

		void ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args);
		void PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args);
		void RebuildMaterialOverrides();
		System::Collections::Generic::List<RawMaterial^>^ _boundRawMaterials;
	};

}
