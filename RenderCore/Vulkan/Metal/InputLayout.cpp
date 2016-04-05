// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"

namespace RenderCore { namespace Metal_Vulkan
{

#if 0
    BoundUniforms::BoundUniforms(const ShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[ShaderStage::Vertex]._reflection     = CreateReflection(shader.GetCompiledVertexShader());
        _stageBindings[ShaderStage::Pixel]._reflection      = CreateReflection(shader.GetCompiledPixelShader());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[ShaderStage::Geometry]._reflection   = CreateReflection(*geoShader);
        }
    }

    BoundUniforms::BoundUniforms(const DeepShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[ShaderStage::Vertex]._reflection     = CreateReflection(shader.GetCompiledVertexShader());
        _stageBindings[ShaderStage::Pixel]._reflection      = CreateReflection(shader.GetCompiledPixelShader());
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[ShaderStage::Geometry]._reflection   = CreateReflection(*geoShader);
        }
        _stageBindings[ShaderStage::Hull]._reflection       = CreateReflection(shader.GetCompiledHullShader());
        _stageBindings[ShaderStage::Domain]._reflection     = CreateReflection(shader.GetCompiledDomainShader());
    }

	BoundUniforms::BoundUniforms(const CompiledShaderByteCode& shader)
    {
            //  In this case, we're binding with a single shader stage
        ShaderStage::Enum stage = shader.GetStage();
        if (stage < dimof(_stageBindings)) {
            _stageBindings[stage]._reflection = CreateReflection(shader);
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

    static NativeFormat::Enum AsNativeFormat(D3D11_SHADER_TYPE_DESC typeDesc)
    {
        switch (typeDesc.Type) {
        case D3D10_SVT_INT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return NativeFormat::R32G32B32A32_SINT;
                } else if (typeDesc.Columns == 2) {
                    return NativeFormat::R32G32_SINT;
                } else if (typeDesc.Columns == 1) {
                    return NativeFormat::R32_SINT;
                } else {
                    return NativeFormat::Unknown;
                }
            }
            break;
        case D3D10_SVT_UINT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return NativeFormat::R32G32B32A32_UINT;
                } else if (typeDesc.Columns == 2) {
                    return NativeFormat::R32G32_UINT;
                } else if (typeDesc.Columns == 1) {
                    return NativeFormat::R32_UINT;
                } else {
                    return NativeFormat::Unknown;
                }
            }
            break;
        case D3D10_SVT_UINT8:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return NativeFormat::R8G8B8A8_UINT;
                } else if (typeDesc.Columns == 2) {
                    return NativeFormat::R8G8B8A8_UINT;
                } else if (typeDesc.Columns == 1) {
                    return NativeFormat::R8G8B8A8_UINT;
                } else {
                    return NativeFormat::Unknown;
                }
            }
            break;
        case D3D10_SVT_FLOAT:
            if (typeDesc.Rows <= 1) {
                if (typeDesc.Columns == 4) {
                    return NativeFormat::R32G32B32A32_FLOAT;
                } else if (typeDesc.Columns == 2) {
                    return NativeFormat::R32G32_FLOAT;
                } else if (typeDesc.Columns == 1) {
                    return NativeFormat::R32_FLOAT;
                } else {
                    return NativeFormat::Unknown;
                }
            }
            break;
        case D3D11_SVT_DOUBLE:
        default:
            break;
        }
        return NativeFormat::Unknown;
    }

    bool BoundUniforms::BindConstantBuffers(unsigned uniformsStream, std::initializer_list<const char*> cbs)
    {
            // expecting this method to be called before any other BindConstantBuffers 
            // operations for this uniformsStream (because we start from a zero index)
        #if defined(_DEBUG)
            for (unsigned c=0; c<ShaderStage::Max; ++c)
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
            for (unsigned c=0; c<ShaderStage::Max; ++c)
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
            for (unsigned c=0; c<ShaderStage::Max; ++c)
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
            for (unsigned c=0; c<ShaderStage::Max; ++c)
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
		for (unsigned c=0; c<ShaderStage::Max; ++c) {
			_stageBindings[c]._shaderConstantBindings.clear();
			_stageBindings[c]._shaderResourceBindings.clear();
			_stageBindings[c]._reflection = copyFrom._stageBindings[c]._reflection;
		}
	}

	intrusive_ptr<ID3D::ShaderReflection> BoundUniforms::GetReflection(ShaderStage::Enum stage)
	{
		return _stageBindings[stage]._reflection;
	}

#endif

}}

