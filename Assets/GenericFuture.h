// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"

namespace Assets
{
	/// <summary>Records the status of asynchronous operation, very much like a std::promise<AssetState></summary>
	class IAsyncMarker
	{
	public:
		virtual AssetState		GetAssetState() const = 0;
		virtual AssetState		StallWhilePending() const = 0;
		virtual ~IAsyncMarker();
	};

    class GenericFuture : public IAsyncMarker, public std::enable_shared_from_this<GenericFuture>
    {
    public:
        AssetState		GetAssetState() const { return _state; }
        AssetState		StallWhilePending() const;
        const char*     Initializer() const;  // "initializer" interface only provided in debug builds, and only intended for debugging

        GenericFuture(AssetState state = AssetState::Pending);
        ~GenericFuture();

		GenericFuture(GenericFuture&&) = delete;
		GenericFuture& operator=(GenericFuture&&) = delete;
		GenericFuture(const GenericFuture&) = delete;
		GenericFuture& operator=(const GenericFuture&) = delete;

		void	SetState(AssetState newState);
		void	SetInitializer(const ResChar initializer[]);

	private:
		AssetState _state;
		DEBUG_ONLY(ResChar _initializer[MaxPath];)
    };

}
