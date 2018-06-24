// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Buffer.h"
#include "TextureView.h"
#include "DeviceContext.h"
#include "DX11Utils.h"
#include "Shader.h"
#include "ObjectFactory.h"
#include "Format.h"
#include "../../Format.h"
#include "../../Types.h"
#include "../../BufferView.h"
#include "../../RenderUtils.h"
#include "../../ShaderService.h"
#include "../../UniformsStream.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/MemoryUtils.h"
#include <D3D11Shader.h>

namespace RenderCore { namespace Metal_DX11
{
	static intrusive_ptr<ID3D::InputLayout> BuildInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader)
	{
		auto byteCode = shader.GetByteCode();

		D3D11_INPUT_ELEMENT_DESC nativeLayout[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		for (unsigned c = 0; c<std::min(dimof(nativeLayout), layout.size()); ++c) {
			nativeLayout[c].SemanticName = layout.first[c]._semanticName.c_str();
			nativeLayout[c].SemanticIndex = layout.first[c]._semanticIndex;
			nativeLayout[c].Format = AsDXGIFormat(layout.first[c]._nativeFormat);
			nativeLayout[c].InputSlot = layout.first[c]._inputSlot;
			nativeLayout[c].AlignedByteOffset = layout.first[c]._alignedByteOffset;
			nativeLayout[c].InputSlotClass = D3D11_INPUT_CLASSIFICATION(layout.first[c]._inputSlotClass);
			nativeLayout[c].InstanceDataStepRate = layout.first[c]._instanceDataStepRate;
		}

		return GetObjectFactory().CreateInputLayout(
			nativeLayout, (unsigned)std::min(dimof(nativeLayout), layout.size()),
			byteCode.begin(), byteCode.size());
	}

	static std::vector<std::pair<uint64_t, std::pair<const char*, UINT>>> GetInputParameters(ID3D::ShaderReflection& reflection)
	{
		std::vector<std::pair<uint64_t, std::pair<const char*, UINT>>> result;
		unsigned paramCount = reflection.GetNumInterfaceSlots();
		for (unsigned p=0;p<paramCount; ++p) {
			D3D11_SIGNATURE_PARAMETER_DESC desc;
			auto hresult = reflection.GetInputParameterDesc(p, &desc);
			assert(SUCCEEDED(hresult));
			if (!desc.SemanticName) continue;
			auto hash = Hash64(desc.SemanticName) + desc.SemanticIndex;
			result.push_back({hash, {desc.SemanticName, desc.SemanticIndex}});
		}
		std::sort(result.begin(), result.end(), CompareFirst<uint64_t, std::pair<const char*, UINT>>());
		return result;
	}

	static intrusive_ptr<ID3D::InputLayout> BuildInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const CompiledShaderByteCode& shader)
	{
		auto byteCode = shader.GetByteCode();
		auto reflection = CreateReflection(shader);
		auto inputParameters = GetInputParameters(*reflection);

		UINT accumulatingOffset = 0; 

		D3D11_INPUT_ELEMENT_DESC nativeLayout[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		for (unsigned c = 0; c<std::min(dimof(nativeLayout), layout.size()); ++c) {
			// We have to lookup the name of an input parameter that matches the hash,
			// because CreateInputLayout requires the full semantic name
			auto i = LowerBound(inputParameters, layout[c]._semanticHash);
			if (i==inputParameters.end() || i->first == layout[c]._semanticHash)
				continue;

			nativeLayout[c].SemanticName = i->second.first;
			nativeLayout[c].SemanticIndex = i->second.second;
			nativeLayout[c].Format = AsDXGIFormat(layout.first[c]._nativeFormat);
			nativeLayout[c].InputSlot = 0;
			nativeLayout[c].AlignedByteOffset = accumulatingOffset;
			nativeLayout[c].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			nativeLayout[c].InstanceDataStepRate = 0;

			accumulatingOffset += BitsPerPixel(layout.first[c]._nativeFormat) / 8;
		}

		return GetObjectFactory().CreateInputLayout(
			nativeLayout, (unsigned)std::min(dimof(nativeLayout), layout.size()),
			byteCode.begin(), byteCode.size());
	}

	static std::vector<unsigned> CalculateVertexStrides(IteratorRange<const MiniInputElementDesc*> layout)
	{
		return std::vector<unsigned> { RenderCore::CalculateVertexStride(layout) };
	}

	void BoundInputLayout::Apply(DeviceContext& context, IteratorRange<const VertexBufferView*> vertexBuffers) const never_throws
	{
		ID3D::Buffer* buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		auto vbCount = std::min(vertexBuffers.size(), _vertexStrides.size());
		assert(vbCount < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		for (unsigned c = 0; c < vbCount; ++c) {
			assert(vertexBuffers[c]._resource);
			auto* res = AsID3DResource(*const_cast<IResource*>(vertexBuffers[c]._resource));
			assert(QueryInterfaceCast<ID3D::Buffer>(res));
			buffers[c] = (ID3D::Buffer*)res;
			offsets[c] = vertexBuffers[c]._offset;
			strides[c] = _vertexStrides[c];
		}
		const unsigned startSlot = 0;
		context.GetUnderlying()->IASetVertexBuffers(startSlot, (UINT)vbCount, buffers, strides, offsets);
		context.GetUnderlying()->IASetInputLayout(_underlying.get());
	}

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const ShaderProgram& shader)
    {
            // need constructor deferring!
        _underlying = BuildInputLayout(layout, shader.GetCompiledCode(ShaderStage::Vertex));
		_vertexStrides = CalculateVertexStrides(layout);
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const InputElementDesc*> layout, const CompiledShaderByteCode& shader)
    {
        _underlying = BuildInputLayout(layout, shader);
		_vertexStrides = CalculateVertexStrides(layout);
    }

	BoundInputLayout::BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const ShaderProgram& shader)
    {
            // need constructor deferring!
        _underlying = BuildInputLayout(layout, shader.GetCompiledCode(ShaderStage::Vertex));
		_vertexStrides = CalculateVertexStrides(layout);
    }

    BoundInputLayout::BoundInputLayout(IteratorRange<const MiniInputElementDesc*> layout, const CompiledShaderByteCode& shader)
    {
        _underlying = BuildInputLayout(layout, shader);
		_vertexStrides = CalculateVertexStrides(layout);
    }

    BoundInputLayout::BoundInputLayout(DeviceContext& context)
    {
        ID3D::InputLayout* rawptr = nullptr;
        context.GetUnderlying()->IAGetInputLayout(&rawptr);
        _underlying = moveptr(rawptr);
		// todo -- getting the vertex strides would require also querying the vertex buffer bindings 
    }

    BoundInputLayout::BoundInputLayout() {}
    BoundInputLayout::~BoundInputLayout() {}

	BoundInputLayout::BoundInputLayout(BoundInputLayout&& moveFrom) never_throws
	: _underlying(std::move(moveFrom._underlying))
	, _vertexStrides(std::move(moveFrom._vertexStrides))
	{
	}

	BoundInputLayout& BoundInputLayout::operator=(BoundInputLayout&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		_vertexStrides = std::move(moveFrom._vertexStrides);
		return *this;
	}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BoundUniforms::BoundUniforms(
		const ShaderProgram& shader,
        const PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0,
        const UniformsStreamInterface& interface1,
        const UniformsStreamInterface& interface2,
        const UniformsStreamInterface& interface3)
    {
		for (unsigned c=0; c<4; ++c)
			_boundUniformBufferSlots[c] = _boundResourceSlots[c] = 0;

		intrusive_ptr<ID3D::ShaderReflection> reflections[(unsigned)ShaderStage::Max];

            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
		for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c) {
			const auto& compiledCode = shader.GetCompiledCode((ShaderStage)c);
			if (compiledCode.GetStage() == (ShaderStage)c)
				reflections[c] = CreateReflection(compiledCode);
		}

		const UniformsStreamInterface* streams[] = { &interface0, &interface1, &interface2, &interface3 };
		for (unsigned streamIdx=0; streamIdx<dimof(streams); ++streamIdx) {
			const auto& stream = *streams[streamIdx];
			for (unsigned slot=0; slot<(unsigned)stream._cbBindings.size(); ++slot) {
				auto bound = BindConstantBuffer(
					MakeIteratorRange(reflections),
					stream._cbBindings[slot]._hashName, slot, streamIdx,
					MakeIteratorRange(stream._cbBindings[slot]._elements));
				if (bound)
					_boundUniformBufferSlots[streamIdx] |= 1ull << uint64_t(slot);
			}
			for (unsigned slot=0; slot<(unsigned)stream._srvBindings.size(); ++slot) {
				auto bound = BindShaderResource(
					MakeIteratorRange(reflections),
					stream._srvBindings[slot], slot, streamIdx);
				if (bound)
					_boundResourceSlots[streamIdx] |= 1ull << uint64_t(slot);
			}
		}
    }

    BoundUniforms::BoundUniforms() 
	{
		for (unsigned c=0; c<4; ++c)
			_boundUniformBufferSlots[c] = _boundResourceSlots[c] = 0;
	}

    BoundUniforms::~BoundUniforms() {}

    bool BoundUniforms::BindConstantBuffer( 
		IteratorRange<const intrusive_ptr<ID3D::ShaderReflection>*> reflections,
		uint64 hashName, unsigned slot, unsigned stream,
        IteratorRange<const ConstantBufferElementDesc*> elements)
    {
		bool functionResult = false;

            //
            //    Look for this constant buffer in the shader interface.
            //        If it exists, let's validate that the input layout is similar
            //        to what the shader expects.
            //
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!reflections[s]) continue;

            D3D11_SHADER_DESC shaderDesc;
            auto hresult = reflections[s]->GetDesc(&shaderDesc);

			if (SUCCEEDED(hresult)) {
				for (unsigned c2=0; c2<shaderDesc.BoundResources; ++c2) {
					D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
					hresult = reflections[s]->GetResourceBindingDesc(c2, &bindingDesc);
					if (SUCCEEDED(hresult)) {
						const auto hash = Hash64(bindingDesc.Name, XlStringEnd(bindingDesc.Name));
						if (hash == hashName) {
							ID3D::ShaderReflectionConstantBuffer* cbReflection = reflections[s]->GetConstantBufferByName(
								bindingDesc.Name);
							if (!cbReflection) 
								continue;

							D3D11_SHADER_BUFFER_DESC cbDesc;
							XlZeroMemory(cbDesc);
							cbReflection->GetDesc(&cbDesc); // when GetConstantBufferByName() fails, it returns a dummy object that doesn't do anything in GetDesc();
							if (!cbDesc.Size) 
								continue;

							assert(Hash64(cbDesc.Name) == hashName);        // double check we got the correct reflection object

							#if defined(_DEBUG)
								for (size_t c=0; c<elements.size(); ++c) {
									ID3D::ShaderReflectionVariable* variable = nullptr;

									// search through to find a variable a name that matches the hash value
									for (unsigned v=0; v<cbDesc.Variables; ++v) {
										auto* var = cbReflection->GetVariableByIndex(v);
										if (!var) continue;
										D3D11_SHADER_VARIABLE_DESC desc;
										hresult = var->GetDesc(&desc);
										if (!SUCCEEDED(hresult)) continue;
										auto h = Hash64(desc.Name);
										if (h == elements[c]._semanticHash) {
											variable = var;
											break;
										}
									}

									assert(variable);
									if (variable) {
										D3D11_SHADER_VARIABLE_DESC variableDesc;
										XlZeroMemory(variableDesc);
										variable->GetDesc(&variableDesc);
										assert(variableDesc.Name!=nullptr);
										assert(variableDesc.StartOffset == elements[c]._offset);
										assert(variableDesc.Size == std::max(1u, elements[c]._arrayElementCount) * BitsPerPixel(elements[c]._nativeFormat) / 8);
									}
								}
							#endif

							StageBinding::Binding newBinding;
							newBinding._shaderSlot = bindingDesc.BindPoint;
							newBinding._inputInterfaceSlot = slot | (stream<<16);
							newBinding._savedCB = MakeConstantBuffer(GetObjectFactory(), cbDesc.Size);
							_stageBindings[s]._shaderConstantBindings.push_back(newBinding);
							functionResult = true;
							break;
						}
					}
				}
			}
        }

        return functionResult;
    }

    bool BoundUniforms::BindShaderResource(
		IteratorRange<const intrusive_ptr<ID3D::ShaderReflection>*> reflections,
		uint64 hashName, unsigned slot, unsigned stream)
    {
        bool functionResult = false;
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!reflections[s]) continue;

            D3D11_SHADER_DESC shaderDesc;
            auto hresult = reflections[s]->GetDesc(&shaderDesc);

			assert(SUCCEEDED(hresult));
			if (SUCCEEDED(hresult)) {
				bool gotBinding = false;
				for (unsigned c=0; c<shaderDesc.BoundResources && !gotBinding; ++c) {
					D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
					hresult = reflections[s]->GetResourceBindingDesc(c, &bindingDesc);
					if (SUCCEEDED(hresult)) {
						const uint64 hash = Hash64(bindingDesc.Name, XlStringEnd(bindingDesc.Name));
						if (hash == hashName) {
							StageBinding::Binding newBinding = {bindingDesc.BindPoint, slot | (stream<<16)};
							_stageBindings[s]._shaderResourceBindings.push_back(newBinding);
							gotBinding = functionResult = true;      // (we should also try to bind against other stages)
						}
					}
				}
			}
        }

        return functionResult;
    }

    void BoundUniforms::Apply(  
		DeviceContext& context, 
        unsigned streamIdx,
        const UniformsStream& stream) const
    {
        Internal::SetConstantBuffersFn scb[(unsigned)ShaderStage::Max] = 
        {
            &ID3D::DeviceContext::VSSetConstantBuffers,
            &ID3D::DeviceContext::PSSetConstantBuffers,
            &ID3D::DeviceContext::GSSetConstantBuffers,
            &ID3D::DeviceContext::HSSetConstantBuffers,
            &ID3D::DeviceContext::DSSetConstantBuffers,
            &ID3D::DeviceContext::CSSetConstantBuffers,
        };

        Internal::SetShaderResourcesFn scr[(unsigned)ShaderStage::Max] = 
        {
            &ID3D::DeviceContext::VSSetShaderResources,
            &ID3D::DeviceContext::PSSetShaderResources,
            &ID3D::DeviceContext::GSSetShaderResources,
            &ID3D::DeviceContext::HSSetShaderResources,
            &ID3D::DeviceContext::DSSetShaderResources,
            &ID3D::DeviceContext::CSSetShaderResources,
        };

        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            const StageBinding& stage = _stageBindings[s];

            {
                unsigned lowestShaderSlot = ~unsigned(0x0), highestShaderSlot = 0;
                auto* currentCBS = context._currentCBs[s];
                uint32 setMask = 0x0;
                for (auto   i =stage._shaderConstantBindings.begin(); 
                            i!=stage._shaderConstantBindings.end(); ++i) {

					if ((i->_inputInterfaceSlot >> 16) != streamIdx)
						continue;

                    unsigned slot = i->_inputInterfaceSlot & 0xff;
                    if (slot < stream._constantBuffers.size()) {
						const auto& cb = stream._constantBuffers[slot];
                        if (cb._prebuiltBuffer) {

							auto* res = checked_cast<const Resource*>(cb._prebuiltBuffer)->_underlying.get();
							assert(QueryInterfaceCast<ID3D::Buffer>(res));

                            setMask |= 1 << (i->_shaderSlot);
                            if (res != currentCBS[i->_shaderSlot]) {
                                currentCBS[i->_shaderSlot] = (ID3D::Buffer*)res;
                                lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                                highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                            }

                        } else if (cb._packet.size()) {

                            i->_savedCB.Update(context, cb._packet.begin(), cb._packet.size());
                            setMask |= 1 << (i->_shaderSlot);
                            if (i->_savedCB.GetUnderlying() != currentCBS[i->_shaderSlot]) {
                                currentCBS[i->_shaderSlot] = i->_savedCB.GetUnderlying();
                                lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                                highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                            }

                        }
                    }

                }

                if (lowestShaderSlot <= highestShaderSlot) {
                    
                        //  We have to clear out the pointers to CBs that aren't explicit set. This is because
                        //  we don't know if those pointers are still valid currently -- they may have been deleted
                        //  somewhere else in the pipeline
                    for (unsigned c=lowestShaderSlot; c<=highestShaderSlot; ++c)
                        if (!(setMask & (1<<c)))
                            currentCBS[c] = nullptr;

                    Internal::SetConstantBuffersFn fn = scb[s];
                    (context.GetUnderlying()->*fn)(lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1, &currentCBS[lowestShaderSlot]);
                }
            }

            {
                unsigned lowestShaderSlot = ~unsigned(0x0), highestShaderSlot = 0;
                auto* currentSRVs = context._currentSRVs[s];
                uint32 setMask = 0;
                for (auto   i =stage._shaderResourceBindings.cbegin(); 
                            i!=stage._shaderResourceBindings.cend(); ++i) {

					if ((i->_inputInterfaceSlot >> 16) != streamIdx)
						continue;

                    unsigned slot = i->_inputInterfaceSlot & 0xff;
                    if (slot < stream._resources.size() && stream._resources[slot]) {
                        currentSRVs[i->_shaderSlot] = ((ShaderResourceView*)stream._resources[slot])->GetUnderlying();
                        lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                        highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                        setMask |= 1<<(i->_shaderSlot);
                    }
                }

                if (lowestShaderSlot <= highestShaderSlot) {

                    for (unsigned c=lowestShaderSlot; c<=highestShaderSlot; ++c)
                        if (!(setMask & (1<<c)))
                            currentSRVs[c] = nullptr;

                    Internal::SetShaderResourcesFn fn = scr[s];
                    (context.GetUnderlying()->*fn)(
                        lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1,
                        &currentSRVs[lowestShaderSlot]);
                }
            }
        }
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
		ID3D::ShaderResourceView* srv = nullptr;
        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Vertex]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
				context.GetUnderlying()->VSSetShaderResources(b._shaderSlot, 1, &srv);
                context._currentSRVs[(unsigned)ShaderStage::Vertex][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Pixel]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
				context.GetUnderlying()->PSSetShaderResources(b._shaderSlot, 1, &srv);
                context._currentSRVs[(unsigned)ShaderStage::Pixel][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Geometry]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.GetUnderlying()->GSSetShaderResources(b._shaderSlot, 1, &srv);
                context._currentSRVs[(unsigned)ShaderStage::Geometry][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Compute]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.GetUnderlying()->CSSetShaderResources(b._shaderSlot, 1, &srv);
                context._currentSRVs[(unsigned)ShaderStage::Compute][b._shaderSlot] = nullptr;
            }
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void BoundClassInterfaces::Bind(uint64 hashName, unsigned bindingArrayIndex, const char instance[])
    {
        for (auto s=0; s<(unsigned)ShaderStage::Max; ++s) {
            if (!_stageBindings[s]._linkage || !_stageBindings[s]._reflection) continue;

            bool gotoNextStage = false;
            D3D11_SHADER_DESC shaderDesc;
            auto hresult = _stageBindings[s]._reflection->GetDesc(&shaderDesc);
            if (SUCCEEDED(hresult)) {
                for (unsigned c=0; c<shaderDesc.ConstantBuffers && !gotoNextStage; ++c) {
				    auto cb = _stageBindings[s]._reflection->GetConstantBufferByIndex(c);
				    if (cb) {
                        D3D11_SHADER_BUFFER_DESC cbDesc;
                        hresult = cb->GetDesc(&cbDesc);
                        if (SUCCEEDED(hresult)) {
                            for (unsigned q=0; q<cbDesc.Variables; ++q) {
                                auto var = cb->GetVariableByIndex(q);
                                if (!var) continue;

                                auto type = var->GetType();
                                if (!type) continue;

                                D3D11_SHADER_TYPE_DESC typeDesc;
                                hresult = type->GetDesc(&typeDesc);
                                if (!SUCCEEDED(hresult)) continue;

                                if (typeDesc.Class != D3D11_SVC_INTERFACE_POINTER) continue;

                                D3D11_SHADER_VARIABLE_DESC varDesc;
                                hresult  = var->GetDesc(&varDesc);
                                if (!SUCCEEDED(hresult)) continue;

                                const uint64 hash = Hash64(varDesc.Name, XlStringEnd(varDesc.Name));
					            if (hash != hashName) continue;

                                auto finalSlot = var->GetInterfaceSlot(bindingArrayIndex);
                                if (finalSlot >= _stageBindings[s]._classInstanceArray.size()) continue;

                                ID3D::ClassInstance* classInstance = nullptr;
                                hresult = _stageBindings[s]._linkage->CreateClassInstance(
                                    instance, 
                                    0, 0, 0, 0, 
                                    &classInstance);

                                if (!SUCCEEDED(hresult)) continue;
                                _stageBindings[s]._classInstanceArray[finalSlot] = moveptr(classInstance);

                                gotoNextStage = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    const std::vector<intrusive_ptr<ID3D::ClassInstance>>& BoundClassInterfaces::GetClassInstances(ShaderStage stage) const
    {
        return _stageBindings[unsigned(stage)]._classInstanceArray;
    }

    BoundClassInterfaces::BoundClassInterfaces(const ShaderProgram& shader)
    {
		for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c) {
			auto* classLinkage = shader.GetClassLinkage((ShaderStage)c);
			if (classLinkage) {
				_stageBindings[c]._linkage = classLinkage; 
				_stageBindings[c]._reflection = CreateReflection(shader.GetCompiledCode((ShaderStage)c));
				if (_stageBindings[c]._reflection)
					_stageBindings[c]._classInstanceArray.resize(
						_stageBindings[c]._reflection->GetNumInterfaceSlots(), nullptr);
			}
		}
    }

    BoundClassInterfaces::BoundClassInterfaces() {}
    BoundClassInterfaces::~BoundClassInterfaces() {}

    BoundClassInterfaces::BoundClassInterfaces(BoundClassInterfaces&& moveFrom)
    {
        for (unsigned c=0; c<dimof(_stageBindings); ++c)
            _stageBindings[c] = std::move(moveFrom._stageBindings[c]);
    }

    BoundClassInterfaces& BoundClassInterfaces::operator=(BoundClassInterfaces&& moveFrom)
    {
        for (unsigned c=0; c<dimof(_stageBindings); ++c)
            _stageBindings[c] = std::move(moveFrom._stageBindings[c]);
        return *this;
    }



    BoundClassInterfaces::StageBinding::StageBinding() {}
    BoundClassInterfaces::StageBinding::~StageBinding() {}

    BoundClassInterfaces::StageBinding::StageBinding(StageBinding&& moveFrom)
    : _reflection(std::move(moveFrom._reflection))
    , _linkage(std::move(moveFrom._linkage))
    , _classInstanceArray(std::move(moveFrom._classInstanceArray))
    {}

    auto BoundClassInterfaces::StageBinding::operator=(StageBinding&& moveFrom) -> StageBinding&
    {
        _reflection = std::move(moveFrom._reflection);
        _linkage = std::move(moveFrom._linkage);
        _classInstanceArray = std::move(moveFrom._classInstanceArray);
        return *this;
    }

	BoundClassInterfaces::StageBinding::StageBinding(const StageBinding& copyFrom)
	: _reflection(copyFrom._reflection)
	, _linkage(copyFrom._linkage)
	, _classInstanceArray(copyFrom._classInstanceArray)
	{}

	auto BoundClassInterfaces::StageBinding::operator=(const StageBinding& copyFrom) -> StageBinding&
	{
		_reflection = copyFrom._reflection;
		_linkage = copyFrom._linkage;
		_classInstanceArray = copyFrom._classInstanceArray;
		return *this;
	}

	void NumericUniformsInterface::Reset() {}

	NumericUniformsInterface::NumericUniformsInterface(DeviceContext& context, ShaderStage shaderStage)
	: _context(&context)
	, _stage(shaderStage)
	{
		_setUnorderedAccessViews = nullptr;
		switch (shaderStage)
		{
		case ShaderStage::Vertex:
			_setShaderResources = &ID3D::DeviceContext::VSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::VSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::VSSetConstantBuffers;
			break;
		case ShaderStage::Pixel:
			_setShaderResources = &ID3D::DeviceContext::PSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::PSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::PSSetConstantBuffers;
			break;
		case ShaderStage::Geometry:
			_setShaderResources = &ID3D::DeviceContext::GSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::GSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::GSSetConstantBuffers;
			break;
		case ShaderStage::Hull:
			_setShaderResources = &ID3D::DeviceContext::HSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::HSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::HSSetConstantBuffers;
			break;
		case ShaderStage::Domain:
			_setShaderResources = &ID3D::DeviceContext::DSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::DSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::DSSetConstantBuffers;
			break;
		case ShaderStage::Compute:
			_setShaderResources = &ID3D::DeviceContext::CSSetShaderResources;
			_setSamplers = &ID3D::DeviceContext::CSSetSamplers;
			_setConstantBuffers = &ID3D::DeviceContext::CSSetConstantBuffers;
			_setUnorderedAccessViews = &ID3D::DeviceContext::CSSetUnorderedAccessViews;
			break;
		default:
			assert(0);
			_setShaderResources = nullptr;
			_setSamplers = nullptr;
			_setConstantBuffers = nullptr;
			break;
		}
	}

	NumericUniformsInterface::NumericUniformsInterface()
	{
		_context = nullptr;
		_stage = ShaderStage::Null;
		_setShaderResources = nullptr;
		_setSamplers = nullptr;
		_setConstantBuffers = nullptr;
		_setUnorderedAccessViews = nullptr;
	}

    NumericUniformsInterface::~NumericUniformsInterface()
	{
	}

#if 0
    static Format AsNativeFormat(D3D11_SHADER_TYPE_DESC typeDesc)
    {
        switch (typeDesc.Type) {
        case D3D10_SVT_INT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return Format::R32G32B32A32_SINT;
                } else if (typeDesc.Columns == 2) {
                    return Format::R32G32_SINT;
                } else if (typeDesc.Columns == 1) {
                    return Format::R32_SINT;
                } else {
                    return Format::Unknown;
                }
            }
            break;
        case D3D10_SVT_UINT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return Format::R32G32B32A32_UINT;
                } else if (typeDesc.Columns == 2) {
                    return Format::R32G32_UINT;
                } else if (typeDesc.Columns == 1) {
                    return Format::R32_UINT;
                } else {
                    return Format::Unknown;
                }
            }
            break;
        case D3D10_SVT_UINT8:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return Format::R8G8B8A8_UINT;
                } else if (typeDesc.Columns == 2) {
                    return Format::R8G8B8A8_UINT;
                } else if (typeDesc.Columns == 1) {
                    return Format::R8G8B8A8_UINT;
                } else {
                    return Format::Unknown;
                }
            }
            break;
        case D3D10_SVT_FLOAT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return Format::R32G32B32A32_FLOAT;
                } else if (typeDesc.Columns == 2) {
                    return Format::R32G32_FLOAT;
                } else if (typeDesc.Columns == 1) {
                    return Format::R32_FLOAT;
                } else {
                    return Format::Unknown;
                }
            }
            break;
        case D3D11_SVT_DOUBLE:
        default:
            break;
        }
        return Format::Unknown;
    }
#endif

}}

