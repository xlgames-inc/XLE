// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "../../ConsoleRig/AttachableInternal.h"
#include "../../BufferUploads/IBufferUploads.h"

namespace RenderCore { namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(RenderCore::IDevice& device)
    {
        BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
        _bufferUploads = BufferUploads::CreateManager(&device);

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    Services::~Services()
    {
        _bufferUploads.reset();
        BufferUploads::DetachLibrary();
        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void Services::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
    }

    void Services::DetachCurrentModule()
    {
        assert(s_instance==this);
        s_instance = nullptr;
    }

}}

