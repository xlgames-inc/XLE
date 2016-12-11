// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredConstruction.h"
#include "AssetUtils.h"
#include "../ConsoleRig/Log.h"

namespace Assets
{
	AssetState DeferredConstruction::GetAssetState() const
	{
		if (_upstreamMarker)
			return _upstreamMarker->GetAssetState();

		// If there no upstream marker, then we knew the state at construction.
		// It's either ready or invalid depending on whether there's an constructor
		// in our function table
		return _fns.IsEmpty() ? AssetState::Invalid : AssetState::Ready;
	}

	AssetState DeferredConstruction::StallWhilePending() const
	{
		if (_upstreamMarker)
			return _upstreamMarker->StallWhilePending();
		return _fns.IsEmpty() ? AssetState::Invalid : AssetState::Ready;
	}

	DeferredConstruction::~DeferredConstruction() {}

	namespace Internal 
	{
		void TryRecompile(const Assets::Exceptions::FormatError& e)
		{
			if (e.GetReason() == ::Assets::Exceptions::FormatError::Reason::UnsupportedVersion)
				LogWarning << "Data file is unsupported version! Recommend recompile";
		}
	}
}
