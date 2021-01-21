// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <functional>

namespace Assets 
{

    class ArtifactFuture;
	void QueueCompileOperation(
		const std::shared_ptr<::Assets::ArtifactFuture>& future,
		std::function<void(::Assets::ArtifactFuture&)>&& operation);
		
}

