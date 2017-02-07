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
#include "../../Format.h"
#include "../../Types.h"
#include "../../RenderUtils.h"
#include "../../../Utility/StringUtils.h"
#include "../../../Utility/MemoryUtils.h"
#include <D3D11Shader.h>

namespace RenderCore { namespace Metal_DX11
{

    BoundInputLayout::BoundInputLayout(const InputLayout& layout, const ShaderProgram& shader)
    {
            // need constructor deferring!
        _underlying = BuildInputLayout(layout, shader.GetCompiledVertexShader());
    }

    BoundInputLayout::BoundInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader)
    {
        _underlying = BuildInputLayout(layout, shader);
    }

    BoundInputLayout::BoundInputLayout(DeviceContext& context)
    {
        ID3D::InputLayout* rawptr = nullptr;
        context.GetUnderlying()->IAGetInputLayout(&rawptr);
        _underlying = moveptr(rawptr);
    }

    BoundInputLayout::BoundInputLayout() {}
    BoundInputLayout::~BoundInputLayout() {}

	BoundInputLayout::BoundInputLayout(BoundInputLayout&& moveFrom) never_throws
	: _underlying(std::move(moveFrom._underlying))
	{
	}

	BoundInputLayout& BoundInputLayout::operator=(BoundInputLayout&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		return *this;
	}

    intrusive_ptr<ID3D::InputLayout>   BoundInputLayout::BuildInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader)
    {
        auto byteCode = shader.GetByteCode();

        const unsigned MaxInputLayoutElements = 64;

            //
            //      Our format is almost identical (except std::string -> const char*)
            //
        D3D11_INPUT_ELEMENT_DESC nativeLayout[MaxInputLayoutElements];
        for (unsigned c=0; c<std::min(dimof(nativeLayout), layout.second); ++c) {
            nativeLayout[c].SemanticName             = layout.first[c]._semanticName.c_str();
            nativeLayout[c].SemanticIndex            = layout.first[c]._semanticIndex;
            nativeLayout[c].Format                   = DXGI_FORMAT(layout.first[c]._nativeFormat);
            nativeLayout[c].InputSlot                = layout.first[c]._inputSlot;
            nativeLayout[c].AlignedByteOffset        = layout.first[c]._alignedByteOffset;
            nativeLayout[c].InputSlotClass           = D3D11_INPUT_CLASSIFICATION(layout.first[c]._inputSlotClass);
            nativeLayout[c].InstanceDataStepRate     = layout.first[c]._instanceDataStepRate;
        }

        return GetObjectFactory().CreateInputLayout(
            nativeLayout, (unsigned)std::min(dimof(nativeLayout), layout.second), 
            byteCode.first, byteCode.second);
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BoundUniforms::BoundUniforms(const ShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[(unsigned)ShaderStage::Vertex]._reflection     = CreateReflection(shader.GetCompiledVertexShader());
        _stageBindings[(unsigned)ShaderStage::Pixel]._reflection      = CreateReflection(shader.GetCompiledPixelShader());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[(unsigned)ShaderStage::Geometry]._reflection   = CreateReflection(*geoShader);
        }
    }

    BoundUniforms::BoundUniforms(const DeepShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[(unsigned)ShaderStage::Vertex]._reflection     = CreateReflection(shader.GetCompiledVertexShader());
        _stageBindings[(unsigned)ShaderStage::Pixel]._reflection      = CreateReflection(shader.GetCompiledPixelShader());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[(unsigned)ShaderStage::Geometry]._reflection   = CreateReflection(*geoShader);
        }
        _stageBindings[(unsigned)ShaderStage::Hull]._reflection       = CreateReflection(shader.GetCompiledHullShader());
        _stageBindings[(unsigned)ShaderStage::Domain]._reflection     = CreateReflection(shader.GetCompiledDomainShader());
    }

	BoundUniforms::BoundUniforms(const CompiledShaderByteCode& shader)
    {
            //  In this case, we're binding with a single shader stage
        auto stage = shader.GetStage();
        if ((unsigned)stage < dimof(_stageBindings)) {
            _stageBindings[(unsigned)stage]._reflection = CreateReflection(shader);
        }
    }

    BoundUniforms::BoundUniforms() {}

    BoundUniforms::~BoundUniforms() {}

    BoundUniforms::BoundUniforms(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_stageBindings); ++s)
            _stageBindings[s] = copyFrom._stageBindings[s];
    }

    BoundUniforms& BoundUniforms::operator=(const BoundUniforms& copyFrom)
    {
        for (unsigned s=0; s<dimof(_stageBindings); ++s)
            _stageBindings[s] = copyFrom._stageBindings[s];
        return *this;
    }

    BoundUniforms::BoundUniforms(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_stageBindings); ++s)
            _stageBindings[s] = std::move(moveFrom._stageBindings[s]);
    }

    BoundUniforms& BoundUniforms::operator=(BoundUniforms&& moveFrom)
    {
        for (unsigned s=0; s<dimof(_stageBindings); ++s)
            _stageBindings[s] = std::move(moveFrom._stageBindings[s]);
        return *this;
    }
    
    BoundUniforms::StageBinding::StageBinding() {}
    BoundUniforms::StageBinding::~StageBinding() {}

    BoundUniforms::StageBinding::StageBinding(StageBinding&& moveFrom)
    :   _reflection(std::move(moveFrom._reflection))
    ,   _shaderConstantBindings(std::move(moveFrom._shaderConstantBindings))
    ,   _shaderResourceBindings(std::move(moveFrom._shaderResourceBindings))
    {
    }

    BoundUniforms::StageBinding& BoundUniforms::StageBinding::operator=(BoundUniforms::StageBinding&& moveFrom)
    {
        _reflection = std::move(moveFrom._reflection);
        _shaderConstantBindings = std::move(moveFrom._shaderConstantBindings);
        _shaderResourceBindings = std::move(moveFrom._shaderResourceBindings);
        return *this;
    }

	BoundUniforms::StageBinding::StageBinding(const StageBinding& copyFrom)
	: _reflection(copyFrom._reflection)
	, _shaderConstantBindings(copyFrom._shaderConstantBindings)
	, _shaderResourceBindings(copyFrom._shaderResourceBindings)
	{
	}

	BoundUniforms::StageBinding& BoundUniforms::StageBinding::operator=(const StageBinding& copyFrom)
	{
		_reflection = copyFrom._reflection;
		_shaderConstantBindings = copyFrom._shaderConstantBindings;
		_shaderResourceBindings = copyFrom._shaderResourceBindings;
		return *this;
	}
    

    bool BoundUniforms::BindConstantBuffer( uint64 hashName, unsigned slot, unsigned stream,
                                            const ConstantBufferLayoutElement elements[], size_t elementCount)
    {
        bool functionResult = false;
            //
            //    Look for this constant buffer in the shader interface.
            //        If it exists, let's validate that the input layout is similar
            //        to what the shader expects.
            //
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!_stageBindings[s]._reflection) continue;

            D3D11_SHADER_DESC shaderDesc;
            auto hresult = _stageBindings[s]._reflection->GetDesc(&shaderDesc);

			if (SUCCEEDED(hresult)) {
				for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {
					D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
					HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDesc(c, &bindingDesc);
					if (SUCCEEDED(hresult)) {
						const uint64 hash = Hash64(bindingDesc.Name, XlStringEnd(bindingDesc.Name));
						if (hash == hashName) {
							ID3D::ShaderReflectionConstantBuffer* cbReflection = _stageBindings[s]._reflection->GetConstantBufferByName(
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
								for (size_t c=0; c<elementCount; ++c) {
									ID3D::ShaderReflectionVariable* variable = 
										cbReflection->GetVariableByName(elements[c]._name);
									assert(variable);
									if (variable) {
										D3D11_SHADER_VARIABLE_DESC variableDesc;
										XlZeroMemory(variableDesc);
										variable->GetDesc(&variableDesc);
										assert(variableDesc.Name!=nullptr);
										assert(variableDesc.StartOffset == elements[c]._offset);
										assert(variableDesc.Size == std::max(1u, elements[c]._arrayCount) * BitsPerPixel(elements[c]._format) / 8);
									}
								}
							#endif
							(void)elements; (void)elementCount; // (not used in release)

							StageBinding::Binding newBinding;
							newBinding._shaderSlot = bindingDesc.BindPoint;
							newBinding._inputInterfaceSlot = slot | (stream<<16);
							newBinding._savedCB = ConstantBuffer(nullptr, cbDesc.Size);
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

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs)
    {
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        #if defined(_DEBUG)
            for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c)
                for (const auto& i:_stageBindings[c]._shaderConstantBindings)
                    assert((i._inputInterfaceSlot>>16) != uniformsStream);
        #endif

        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(Hash64(*c), unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<uint64> cbs)
    {
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        #if defined(_DEBUG)
            for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c)
                for (const auto& i:_stageBindings[c]._shaderConstantBindings)
                    assert((i._inputInterfaceSlot>>16) != uniformsStream);
        #endif

        bool result = true;
        for (auto c=cbs.begin(); c<cbs.end(); ++c)
            result &= BindConstantBuffer(*c, unsigned(c-cbs.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<const char*> res)
    {
        #if defined(_DEBUG)
            for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c)
                for (const auto& i:_stageBindings[c]._shaderResourceBindings)
                    assert((i._inputInterfaceSlot>>16) != uniformsStream);
        #endif

        bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(Hash64(*c), unsigned(c-res.begin()), uniformsStream);
        return result;
    }

    bool BoundUniforms::BindShaderResources(unsigned uniformsStream, std::initializer_list<uint64> res)
    {
        #if defined(_DEBUG)
            for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c)
                for (const auto& i:_stageBindings[c]._shaderResourceBindings)
                    assert((i._inputInterfaceSlot>>16) != uniformsStream);
        #endif

        bool result = true;
        for (auto c=res.begin(); c<res.end(); ++c)
            result &= BindShaderResource(*c, unsigned(c-res.begin()), uniformsStream);
        return result;
    }

	void BoundUniforms::CopyReflection(const BoundUniforms& copyFrom)
	{
		for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c) {
			_stageBindings[c]._shaderConstantBindings.clear();
			_stageBindings[c]._shaderResourceBindings.clear();
			_stageBindings[c]._reflection = copyFrom._stageBindings[c]._reflection;
		}
	}

	intrusive_ptr<ID3D::ShaderReflection> BoundUniforms::GetReflection(ShaderStage stage)
	{
		return _stageBindings[(unsigned)stage]._reflection;
	}

    ConstantBufferLayout::ConstantBufferLayout() { _size = 0; _elementCount = 0; }
    ConstantBufferLayout::ConstantBufferLayout(ConstantBufferLayout&& moveFrom)
    :   _elements(std::move(moveFrom._elements))
    ,   _elementCount(moveFrom._elementCount)
    ,   _size(moveFrom._size)
    {}
    ConstantBufferLayout& ConstantBufferLayout::operator=(ConstantBufferLayout&& moveFrom)
    {
        _elements = std::move(moveFrom._elements);
        _elementCount = moveFrom._elementCount;
        _size = moveFrom._size;
        return *this;
    }

#pragma warning(disable:4345) // behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized

    ConstantBufferLayout BoundUniforms::GetConstantBufferLayout(const char name[])
    {
        ConstantBufferLayout result;

            //
            //      Find the first instance of a constant buffer with this name
            //      note -- what happens it doesn't match in different stages?
            //
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!_stageBindings[s]._reflection) continue;

            ID3D::ShaderReflectionConstantBuffer* cbReflection = _stageBindings[s]._reflection->GetConstantBufferByName(name);
            if (!cbReflection) 
                continue;

            D3D11_SHADER_BUFFER_DESC cbDesc;
            XlZeroMemory(cbDesc);
            cbReflection->GetDesc(&cbDesc);
            if (!cbDesc.Size) 
                continue;

            result._size = cbDesc.Size;
            result._elements = std::unique_ptr<ConstantBufferLayoutElementHash[]>(new ConstantBufferLayoutElementHash[cbDesc.Variables]);
            result._elementCount = 0;
            for (unsigned c=0; c<cbDesc.Variables; ++c) {
                auto* variable = cbReflection->GetVariableByIndex(c);
                if (!variable) {
                    continue;
                }
                D3D11_SHADER_VARIABLE_DESC variableDesc;
                D3D11_SHADER_TYPE_DESC typeDesc;
                auto hresult = variable->GetDesc(&variableDesc);
                if (!SUCCEEDED(hresult)) {
                    continue;
                }
                auto* type = variable->GetType();
                hresult = type->GetDesc(&typeDesc);
                if (!SUCCEEDED(hresult)) {
                    continue;
                }
                if (typeDesc.Class != D3D10_SVC_SCALAR && typeDesc.Class != D3D10_SVC_VECTOR && typeDesc.Class != D3D10_SVC_MATRIX_ROWS && typeDesc.Class != D3D10_SVC_MATRIX_COLUMNS) {
                    continue;
                }

                ConstantBufferLayoutElementHash newElement;
                newElement._name = Hash64(variableDesc.Name, XlStringEnd(variableDesc.Name));
                newElement._offset = variableDesc.StartOffset;
                newElement._arrayCount = typeDesc.Elements;
                newElement._format = AsNativeFormat(typeDesc);
                if (newElement._format == Format::Unknown) {
                    continue;
                }

                result._elements[result._elementCount++] = newElement;
            }
        }

        return std::move(result);
    }

    std::vector<std::pair<ShaderStage,unsigned>> BoundUniforms::GetConstantBufferBinding(const char name[])
    {
        std::vector<std::pair<ShaderStage,unsigned>> result;
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!_stageBindings[s]._reflection) continue;

            ID3D::ShaderReflectionConstantBuffer* cbReflection = _stageBindings[s]._reflection->GetConstantBufferByName(name);
            if (!cbReflection) 
                continue;

            D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
            HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDescByName(name, &bindingDesc);
            if (SUCCEEDED(hresult)) {
                result.push_back(std::make_pair(ShaderStage(s), bindingDesc.BindPoint));
            }
        }
        return result;
    }

    bool BoundUniforms::BindShaderResource(uint64 hashName, unsigned slot, unsigned stream)
    {
        bool functionResult = false;
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!_stageBindings[s]._reflection) continue;

            D3D11_SHADER_DESC shaderDesc;
            auto hresult = _stageBindings[s]._reflection->GetDesc(&shaderDesc);

			assert(SUCCEEDED(hresult));
			if (SUCCEEDED(hresult)) {
				bool gotBinding = false;
				for (unsigned c=0; c<shaderDesc.BoundResources && !gotBinding; ++c) {
					D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
					HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDesc(c, &bindingDesc);
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

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1) const
    {
        typedef void (__stdcall ID3D::DeviceContext::*SetConstantBuffers)(UINT, UINT, ID3D::Buffer *const *);
        typedef void (__stdcall ID3D::DeviceContext::*SetShaderResources)(UINT, UINT, ID3D11ShaderResourceView *const *);

        SetConstantBuffers scb[(unsigned)ShaderStage::Max] = 
        {
            &ID3D::DeviceContext::VSSetConstantBuffers,
            &ID3D::DeviceContext::PSSetConstantBuffers,
            &ID3D::DeviceContext::GSSetConstantBuffers,
            &ID3D::DeviceContext::HSSetConstantBuffers,
            &ID3D::DeviceContext::DSSetConstantBuffers,
            &ID3D::DeviceContext::CSSetConstantBuffers,
        };

        SetShaderResources scr[(unsigned)ShaderStage::Max] = 
        {
            &ID3D::DeviceContext::VSSetShaderResources,
            &ID3D::DeviceContext::PSSetShaderResources,
            &ID3D::DeviceContext::GSSetShaderResources,
            &ID3D::DeviceContext::HSSetShaderResources,
            &ID3D::DeviceContext::DSSetShaderResources,
            &ID3D::DeviceContext::CSSetShaderResources,
        };

        const UniformsStream* streams[] = { &stream0, &stream1 };

        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            const StageBinding& stage = _stageBindings[s];

            {
                unsigned lowestShaderSlot = ~unsigned(0x0), highestShaderSlot = 0;
                auto* currentCBS = context._currentCBs[s];
                uint32 setMask = 0x0;
                for (auto   i =stage._shaderConstantBindings.begin(); 
                            i!=stage._shaderConstantBindings.end(); ++i) {

                    unsigned slot = i->_inputInterfaceSlot & 0xff;
                    unsigned streamIndex = i->_inputInterfaceSlot >> 16;
                    if (streamIndex < dimof(streams) && slot < streams[streamIndex]->_packetCount) {
                        auto& stream = *streams[streamIndex];
                        if (stream._packets && stream._packets[slot]) {

                            i->_savedCB.Update(context, stream._packets[slot].begin(), stream._packets[slot].size());
                            setMask |= 1 << (i->_shaderSlot);
                            if (i->_savedCB.GetUnderlying() != currentCBS[i->_shaderSlot]) {
                                currentCBS[i->_shaderSlot] = i->_savedCB.GetUnderlying();
                                lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                                highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                            }

                        } else if (stream._prebuiltBuffers && stream._prebuiltBuffers[slot]) {

                            setMask |= 1 << (i->_shaderSlot);
                            if (stream._prebuiltBuffers[slot]->GetUnderlying() != currentCBS[i->_shaderSlot]) {
                                currentCBS[i->_shaderSlot] = stream._prebuiltBuffers[slot]->GetUnderlying();
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

                    SetConstantBuffers fn = scb[s];
                    (context.GetUnderlying()->*fn)(lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1, &currentCBS[lowestShaderSlot]);
                }
            }

            {
                unsigned lowestShaderSlot = ~unsigned(0x0), highestShaderSlot = 0;
                auto* currentSRVs = context._currentSRVs[s];
                uint32 setMask = 0;
                for (auto   i =stage._shaderResourceBindings.cbegin(); 
                            i!=stage._shaderResourceBindings.cend(); ++i) {
                    unsigned slot = i->_inputInterfaceSlot & 0xff;
                    unsigned streamIndex = i->_inputInterfaceSlot >> 16;
                    if (streamIndex < dimof(streams) && slot < streams[streamIndex]->_resourceCount && streams[streamIndex]->_resources[slot]) {
                        currentSRVs[i->_shaderSlot] = streams[streamIndex]->_resources[slot]->GetUnderlying();
                        lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                        highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                        setMask |= 1<<(i->_shaderSlot);
                    }
                }

                if (lowestShaderSlot <= highestShaderSlot) {

                    for (unsigned c=lowestShaderSlot; c<=highestShaderSlot; ++c)
                        if (!(setMask & (1<<c)))
                            currentSRVs[c] = nullptr;

                    SetShaderResources fn = scr[s];
                    (context.GetUnderlying()->*fn)(
                        lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1,
                        &currentSRVs[lowestShaderSlot]);
                }
            }
        }
    }

    void BoundUniforms::UnbindShaderResources(DeviceContext& context, unsigned streamIndex) const
    {
        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Vertex]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.UnbindVS<ShaderResourceView>(b._shaderSlot, 1);
                context._currentSRVs[(unsigned)ShaderStage::Vertex][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Pixel]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.UnbindPS<ShaderResourceView>(b._shaderSlot, 1);
                context._currentSRVs[(unsigned)ShaderStage::Pixel][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Geometry]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.UnbindGS<ShaderResourceView>(b._shaderSlot, 1);
                context._currentSRVs[(unsigned)ShaderStage::Geometry][b._shaderSlot] = nullptr;
            }
        }

        for (const auto& b:_stageBindings[(unsigned)ShaderStage::Compute]._shaderResourceBindings) {
            if ((b._inputInterfaceSlot >> 16)==streamIndex) {
                context.UnbindCS<ShaderResourceView>(b._shaderSlot, 1);
                context._currentSRVs[(unsigned)ShaderStage::Compute][b._shaderSlot] = nullptr;
            }
        }
    }


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
                        auto hresult = cb->GetDesc(&cbDesc);
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
            // only implemented for a few shader stages currently
        if (shader.GetCompiledVertexShader().DynamicLinkingEnabled()) {
            _stageBindings[(unsigned)ShaderStage::Vertex]._reflection 
                = CreateReflection(shader.GetCompiledVertexShader());
            _stageBindings[(unsigned)ShaderStage::Vertex]._linkage = shader.GetVertexShader().GetClassLinkage();
            if (_stageBindings[(unsigned)ShaderStage::Vertex]._reflection)
				_stageBindings[(unsigned)ShaderStage::Vertex]._classInstanceArray.resize(
					_stageBindings[(unsigned)ShaderStage::Vertex]._reflection->GetNumInterfaceSlots(), nullptr);
        }
        
        if (shader.GetCompiledPixelShader().DynamicLinkingEnabled()) {
            _stageBindings[(unsigned)ShaderStage::Pixel]._reflection 
                = CreateReflection(shader.GetCompiledPixelShader());
            _stageBindings[(unsigned)ShaderStage::Pixel]._linkage = shader.GetPixelShader().GetClassLinkage();
			if (_stageBindings[(unsigned)ShaderStage::Pixel]._reflection)
				_stageBindings[(unsigned)ShaderStage::Pixel]._classInstanceArray.resize(
					_stageBindings[(unsigned)ShaderStage::Pixel]._reflection->GetNumInterfaceSlots(), nullptr);
        }
    }

    BoundClassInterfaces::BoundClassInterfaces(const DeepShaderProgram& shader)
    {
            // only implemented for a few shader stages currently
        if (shader.GetCompiledVertexShader().DynamicLinkingEnabled()) {
            _stageBindings[(unsigned)ShaderStage::Vertex]._reflection 
                = CreateReflection(shader.GetCompiledVertexShader());
            _stageBindings[(unsigned)ShaderStage::Vertex]._linkage = shader.GetVertexShader().GetClassLinkage();
			if (_stageBindings[(unsigned)ShaderStage::Vertex]._reflection)
				_stageBindings[(unsigned)ShaderStage::Vertex]._classInstanceArray.resize(
					_stageBindings[(unsigned)ShaderStage::Vertex]._reflection->GetNumInterfaceSlots(), nullptr);
        }
        
        if (shader.GetCompiledPixelShader().DynamicLinkingEnabled()) {
            _stageBindings[(unsigned)ShaderStage::Pixel]._reflection 
                = CreateReflection(shader.GetCompiledPixelShader());
            _stageBindings[(unsigned)ShaderStage::Pixel]._linkage = shader.GetPixelShader().GetClassLinkage();
			if (_stageBindings[(unsigned)ShaderStage::Pixel]._reflection)
				_stageBindings[(unsigned)ShaderStage::Pixel]._classInstanceArray.resize(
					_stageBindings[(unsigned)ShaderStage::Pixel]._reflection->GetNumInterfaceSlots(), nullptr);
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

}}

