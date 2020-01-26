// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "SimpleModelDeform.h"
#include "SkinDeformer.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/AttachablePtr.h"		// (for ConsoleRig::CrossModule::GetInstance)

namespace RenderCore { namespace Techniques
{
	Services* Services::s_instance = nullptr;

	Services::Services(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_device = device;
		_deformOpsFactory = std::make_shared<DeformOperationFactory>();
		_deformOpsFactory->RegisterDeformOperation("skin", SkinDeformer::InstantiationFunction);

		if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::CrossModule::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(*device);
        }
	}

	Services::~Services()
	{
		if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }
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

