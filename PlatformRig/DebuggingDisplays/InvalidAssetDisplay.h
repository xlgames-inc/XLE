// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderOverlays/DebuggingDisplay.h"

namespace Utility { class IHierarchicalProfiler; }

namespace PlatformRig { namespace Overlays
{
	class InvalidAssetDisplay : public RenderOverlays::DebuggingDisplay::IWidget ///////////////////////////////////////////////////////////
	{
	public:
		using IOverlayContext = RenderOverlays::IOverlayContext;
		using Layout = RenderOverlays::DebuggingDisplay::Layout;
		using Interactables = RenderOverlays::DebuggingDisplay::Interactables;
		using InterfaceState = RenderOverlays::DebuggingDisplay::InterfaceState;
		using InputSnapshot = PlatformRig::InputSnapshot;

		void    Render(IOverlayContext& context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState);
		bool    ProcessInput(InterfaceState& interfaceState, const PlatformRig::InputContext& inputContext, const InputSnapshot& input);

		InvalidAssetDisplay();
		~InvalidAssetDisplay();
	};
}}