// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "InputLayout.h"
#include "Shader.h"
#include "Buffer.h"
#include "DeviceContext.h"
#include "DX11Utils.h"
#include "../../RenderUtils.h"
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

    BoundInputLayout::BoundInputLayout() {}
    BoundInputLayout::~BoundInputLayout() {}

    intrusive_ptr<ID3D::InputLayout>   BoundInputLayout::BuildInputLayout(const InputLayout& layout, const CompiledShaderByteCode& shader)
    {
        const void* byteCode = shader.GetByteCode();
        size_t byteCodeSize = shader.GetSize();

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

        return ObjectFactory().CreateInputLayout(
            nativeLayout, (unsigned)std::min(dimof(nativeLayout), layout.second), 
            byteCode, byteCodeSize);
    }

    namespace GlobalInputLayouts
    {
        namespace Detail
        {
            static const unsigned AppendAlignedElement = ~unsigned(0x0);
            InputElementDesc P2CT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32_FLOAT   ),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc PCT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM ),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc P_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT)
            };

            InputElementDesc PC_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM )
            };

            InputElementDesc PT_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "TEXCOORD",   0, NativeFormat::R32G32_FLOAT   )
            };

            InputElementDesc PN_Elements[] = 
            {
                InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT),
                InputElementDesc( "NORMAL",   0, NativeFormat::R32G32B32_FLOAT )
            };
        }

        InputLayout P2CT = std::make_pair(Detail::P2CT_Elements, dimof(Detail::P2CT_Elements));
        InputLayout PCT = std::make_pair(Detail::PCT_Elements, dimof(Detail::PCT_Elements));
        InputLayout P = std::make_pair(Detail::P_Elements, dimof(Detail::P_Elements));
        InputLayout PC = std::make_pair(Detail::PC_Elements, dimof(Detail::PC_Elements));
        InputLayout PT = std::make_pair(Detail::PT_Elements, dimof(Detail::PT_Elements));
        InputLayout PN = std::make_pair(Detail::PN_Elements, dimof(Detail::PN_Elements));
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    BoundUniforms::BoundUniforms(ShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[ShaderStage::Vertex]._reflection     = shader.GetCompiledVertexShader().GetReflection();
        _stageBindings[ShaderStage::Pixel]._reflection      = shader.GetCompiledPixelShader().GetReflection();
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[ShaderStage::Geometry]._reflection   = geoShader->GetReflection();
        }
    }

    BoundUniforms::BoundUniforms(DeepShaderProgram& shader)
    {
            //  In this case, we must bind with every shader stage 
            //      (since a shader program actually reflects the state of the entire stage pipeline) 
        _stageBindings[ShaderStage::Vertex]._reflection     = shader.GetCompiledVertexShader().GetReflection();
        _stageBindings[ShaderStage::Pixel]._reflection      = shader.GetCompiledPixelShader().GetReflection();
        auto* geoShader = shader.GetCompiledGeometryShader();
        if (geoShader) {
            _stageBindings[ShaderStage::Geometry]._reflection   = geoShader->GetReflection();
        }
        _stageBindings[ShaderStage::Hull]._reflection       = shader.GetCompiledHullShader().GetReflection();
        _stageBindings[ShaderStage::Domain]._reflection     = shader.GetCompiledDomainShader().GetReflection();
    }

    BoundUniforms::BoundUniforms(CompiledShaderByteCode& shader)
    {
            //  In this case, we're binding with a single shader stage
        ShaderStage::Enum stage = shader.GetStage();
        if (stage < dimof(_stageBindings)) {
            _stageBindings[stage]._reflection = shader.GetReflection();
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
    
    BoundUniforms::StageBinding::StageBinding() {}
    BoundUniforms::StageBinding::~StageBinding() {}
    

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
            _stageBindings[s]._reflection->GetDesc(&shaderDesc);

            for (unsigned c=0; c<shaderDesc.BoundResources; ++c) {
                D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
                HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDesc(c, &bindingDesc);
                if (SUCCEEDED(hresult)) {
                    const uint64 hash = Hash64(bindingDesc.Name, &bindingDesc.Name[XlStringLen(bindingDesc.Name)]);
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
                newElement._name = Hash64(variableDesc.Name, &variableDesc.Name[XlStringLen(variableDesc.Name)]);
                newElement._offset = variableDesc.StartOffset;
                newElement._arrayCount = typeDesc.Elements;
                newElement._format = AsNativeFormat(typeDesc);
                if (newElement._format == NativeFormat::Unknown) {
                    continue;
                }

                result._elements[result._elementCount++] = newElement;
            }
        }

        return std::move(result);
    }

    std::vector<std::pair<ShaderStage::Enum,unsigned>> BoundUniforms::GetConstantBufferBinding(const char name[])
    {
        std::vector<std::pair<ShaderStage::Enum,unsigned>> result;
        for (unsigned s=0; s<dimof(_stageBindings); ++s) {
            if (!_stageBindings[s]._reflection) continue;

            ID3D::ShaderReflectionConstantBuffer* cbReflection = _stageBindings[s]._reflection->GetConstantBufferByName(name);
            if (!cbReflection) 
                continue;

            D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
            HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDescByName(name, &bindingDesc);
            if (SUCCEEDED(hresult)) {
                result.push_back(std::make_pair(ShaderStage::Enum(s), bindingDesc.BindPoint));
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
            _stageBindings[s]._reflection->GetDesc(&shaderDesc);

            bool gotBinding = false;
            for (unsigned c=0; c<shaderDesc.BoundResources && !gotBinding; ++c) {
                D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
                HRESULT hresult = _stageBindings[s]._reflection->GetResourceBindingDesc(c, &bindingDesc);
                if (SUCCEEDED(hresult)) {
                    const uint64 hash = Hash64(bindingDesc.Name, &bindingDesc.Name[XlStringLen(bindingDesc.Name)]);
                    if (hash == hashName) {
                        StageBinding::Binding newBinding = {bindingDesc.BindPoint, slot | (stream<<16)};
                        _stageBindings[s]._shaderResourceBindings.push_back(newBinding);
                        gotBinding = functionResult = true;      // (we should also try to bind against other stages)
                    }
                }
            }
        }

        return functionResult;
    }

    void BoundUniforms::Apply(  DeviceContext& context, 
                                const UniformsStream& stream0, 
                                const UniformsStream& stream1)
    {
        typedef void (__stdcall ID3D::DeviceContext::*SetConstantBuffers)(UINT, UINT, ID3D::Buffer *const *);
        typedef void (__stdcall ID3D::DeviceContext::*SetShaderResources)(UINT, UINT, ID3D11ShaderResourceView *const *);

        SetConstantBuffers scb[ShaderStage::Max] = 
        {
            &ID3D::DeviceContext::VSSetConstantBuffers,
            &ID3D::DeviceContext::PSSetConstantBuffers,
            &ID3D::DeviceContext::GSSetConstantBuffers,
            &ID3D::DeviceContext::HSSetConstantBuffers,
            &ID3D::DeviceContext::DSSetConstantBuffers,
            &ID3D::DeviceContext::CSSetConstantBuffers,
        };

        SetShaderResources scr[ShaderStage::Max] = 
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
            StageBinding& stage = _stageBindings[s];

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
                    for (unsigned c=lowestShaderSlot; c<=highestShaderSlot; ++c) {
                        if (!(setMask & (1<<c))) {
                            currentCBS[c] = nullptr;
                        }
                    }

                    SetConstantBuffers fn = scb[s];
                    (context.GetUnderlying()->*fn)(lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1, &currentCBS[lowestShaderSlot]);
                }
            }

            {
                unsigned lowestShaderSlot = ~unsigned(0x0), highestShaderSlot = 0;
                ID3D::ShaderResourceView* srvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
                uint32 setMask = 0;
                for (auto   i =stage._shaderResourceBindings.cbegin(); 
                            i!=stage._shaderResourceBindings.cend(); ++i) {
                    unsigned slot = i->_inputInterfaceSlot & 0xff;
                    unsigned streamIndex = i->_inputInterfaceSlot >> 16;
                    if (streamIndex < dimof(streams) && slot < streams[streamIndex]->_resourceCount && streams[streamIndex]->_resources[slot]) {
                        srvs[i->_shaderSlot] = streams[streamIndex]->_resources[slot]->GetUnderlying();
                        lowestShaderSlot = std::min(lowestShaderSlot, i->_shaderSlot);
                        highestShaderSlot = std::max(highestShaderSlot, i->_shaderSlot);
                        setMask |= 1<<(i->_shaderSlot);
                    }
                }

                if (lowestShaderSlot <= highestShaderSlot) {

                    for (unsigned c=lowestShaderSlot; c<=highestShaderSlot; ++c) {
                        if (!(setMask & (1<<c))) {
                            srvs[c] = nullptr;
                        }
                    }

                    SetShaderResources fn = scr[s];
                    (context.GetUnderlying()->*fn)(
                        lowestShaderSlot, highestShaderSlot-lowestShaderSlot+1,
                        &srvs[lowestShaderSlot]);
                }
            }
        }
    }

}}

