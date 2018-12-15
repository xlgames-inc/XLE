// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParsingContext.h"
#include "Techniques.h"
#include "../Metal/InputLayout.h"   // (for UniformsStream)
#include "../Metal/Buffer.h"
#include "../Metal/ObjectFactory.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringFormat.h"
#include <memory>

namespace RenderCore { namespace Techniques
{

    void ParsingContext::SetGlobalCB(
        RenderCore::Metal::DeviceContext& context, unsigned index, 
        const void* newData, size_t dataSize)
    {
        if (index >= dimof(_globalCBs)) {
            return;
        }

		if (!_globalCBs[index]) {
			_globalCBs[index] = std::make_shared<Metal::Buffer>(
				Metal::GetObjectFactory(),
				CreateDesc(
					BindFlag::ConstantBuffer,
					CPUAccess::WriteDynamic,
					GPUAccess::Read,
					LinearBufferDesc::Create(unsigned(dataSize)),
					"GlobalCB"),
				MakeIteratorRange(newData, PtrAdd(newData, dataSize)));

			_globalCBVs[index] = { _globalCBs[index].get() };
		} else {
			auto* buffer = (Metal::Buffer*)_globalCBs[index]->QueryInterface(typeid(Metal::Buffer).hash_code());
			if (buffer)
				buffer->Update(context, newData, dataSize);
		}
    }

	Metal::Buffer&			ParsingContext::GetGlobalTransformCB()
	{
		assert(_globalCBs[0]);
		return *(Metal::Buffer*)_globalCBs[0]->QueryInterface(typeid(Metal::Buffer).hash_code());
	}

    Metal::Buffer&			ParsingContext::GetGlobalStateCB()
	{
		assert(_globalCBs[1]);
		return *(Metal::Buffer*)_globalCBs[1]->QueryInterface(typeid(Metal::Buffer).hash_code());
	}

	const std::shared_ptr<IResource>& ParsingContext::GetGlobalCB(unsigned index)
	{
		assert(index < dimof(_globalCBs));
		return _globalCBs[index];
	}

    void ParsingContext::Process(const ::Assets::Exceptions::RetrievalError& e)
    {
            //  Handle a "invalid asset" and "pending asset" exception that 
            //  occurred during rendering. Normally this will just mean
            //  reporting the assert to the screen.
            //
            // These happen fairly often -- particularly when just starting up, or
            //  when changing rendering settings.
            //  at the moment, this will result in a bunch of allocations -- that's not
            //  ideal during error processing.
        auto* id = e.Initializer();
        
        auto* bufferStart = _stringHelpers->_pendingAssets;
        if (e.State() == ::Assets::AssetState::Invalid)
            bufferStart = _stringHelpers->_invalidAssets;

        static_assert(
            dimof(_stringHelpers->_pendingAssets) == dimof(_stringHelpers->_invalidAssets),
            "Assuming pending and invalid asset buffers are the same length");

        if (!XlFindStringI(bufferStart, id))
            StringMeldAppend(bufferStart, bufferStart + dimof(_stringHelpers->_pendingAssets)) << "," << id;
    }

    std::shared_ptr<IRenderStateDelegate> ParsingContext::SetRenderStateDelegate(
        const std::shared_ptr<IRenderStateDelegate>& stateSetResolver)
    {
        std::shared_ptr<IRenderStateDelegate> oldResolver = std::move(_renderStateDelegate);
        _renderStateDelegate = std::move(stateSetResolver);
        return std::move(oldResolver);
    }

	std::shared_ptr<ITechniqueDelegate> ParsingContext::SetTechniqueDelegate(
        const std::shared_ptr<ITechniqueDelegate>& techniqueDelegate)
    {
        std::shared_ptr<ITechniqueDelegate> oldDelegate = std::move(_techniqueDelegate);
        _techniqueDelegate = std::move(techniqueDelegate);
        return std::move(oldDelegate);
    }

	std::shared_ptr<IMaterialDelegate> ParsingContext::SetMaterialDelegate(const std::shared_ptr<IMaterialDelegate>& materialDelegate)
	{
		std::shared_ptr<IMaterialDelegate> oldDelegate = std::move(_materialDelegate);
        _materialDelegate = std::move(materialDelegate);
        return std::move(oldDelegate);
	}

    ParsingContext::ParsingContext(const TechniqueContext& techniqueContext, AttachmentPool* namedResources, FrameBufferPool* frameBufferPool)
    {
        _techniqueContext = std::make_unique<TechniqueContext>(techniqueContext);
        _renderStateDelegate = _techniqueContext->_defaultRenderStateDelegate;
        _stringHelpers = std::make_unique<StringHelpers>();
        _namedResources = namedResources;
		_frameBufferPool = frameBufferPool;

		for (unsigned c=0; c<dimof(_globalCBs); ++c)
			_globalCBs[c] = nullptr;

        _projectionDesc.reset((ProjectionDesc*)XlMemAlign(sizeof(ProjectionDesc), 16));
        #pragma push_macro("new")
        #undef new
            new(_projectionDesc.get()) ProjectionDesc();
        #pragma pop_macro("new")

		static_assert(dimof(_globalCBs) == dimof(_globalCBVs), "Expecting equivalent array lengths");
        for (unsigned c=0; c<dimof(_globalCBs); ++c)
			_globalCBVs[c] = { _globalCBs[c].get() };
    }

    ParsingContext::~ParsingContext() {}

    ParsingContext::StringHelpers::StringHelpers()
    {
        _errorString[0] = _pendingAssets[0] = _invalidAssets[0] = _quickMetrics[0] = '\0';
    }

}}

