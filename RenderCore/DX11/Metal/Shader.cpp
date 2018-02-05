// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Shader.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "InputLayout.h"

#include "IncludeDX11.h"
#include <D3D11Shader.h>

namespace RenderCore { namespace Metal_DX11
{
    using ::Assets::ResChar;

#if 0

        ////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned BuildNativeDeclaration(
        D3D11_SO_DECLARATION_ENTRY nativeDeclaration[], unsigned nativeDeclarationCount,
        const GeometryShader::StreamOutputInitializers& soInitializers)
    {
        auto finalCount = std::min(nativeDeclarationCount, soInitializers._outputElementCount);
        for (unsigned c=0; c<finalCount; ++c) {
            auto& ele = soInitializers._outputElements[c];
            nativeDeclaration[c].Stream = 0;
            nativeDeclaration[c].SemanticName = ele._semanticName.c_str();
            nativeDeclaration[c].SemanticIndex = ele._semanticIndex;
            nativeDeclaration[c].StartComponent = 0;
            nativeDeclaration[c].ComponentCount = (BYTE)GetComponentCount(GetComponents(ele._nativeFormat));
                // hack -- treat "R16G16B16A16_FLOAT" as a 3 dimensional vector
            if (ele._nativeFormat == Format::R16G16B16A16_FLOAT)
                nativeDeclaration[c].ComponentCount = 3;
            nativeDeclaration[c].OutputSlot = (BYTE)ele._inputSlot;
            assert(nativeDeclaration[c].OutputSlot < soInitializers._outputBufferCount);
        }
        return finalCount;
    }

    GeometryShader::GeometryShader( StringSection<ResChar> initializer,
                                    const StreamOutputInitializers& soInitializers)
    {
            //
            //      We have to append the shader model to the resource name
            //      (if it's not already there)
            //
        ResChar temp[MaxPath];
        if (!XlFindStringI(initializer, "gs_")) {
            StringMeldInPlace(temp) << initializer << ":" GS_DefShaderModel;
            initializer = temp;
        }

        intrusive_ptr<ID3D::GeometryShader> underlying;

        if (soInitializers._outputBufferCount == 0) {

			const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            assert(compiledShader.GetStage() == ShaderStage::Geometry);
            auto byteCode = compiledShader.GetByteCode();
            underlying = GetObjectFactory().CreateGeometryShader(byteCode.begin(), byteCode.size());

        } else {

            assert(soInitializers._outputBufferCount < D3D11_SO_BUFFER_SLOT_COUNT);
            D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
            auto delcCount = BuildNativeDeclaration(nativeDeclaration, dimof(nativeDeclaration), soInitializers);

            auto& objFactory = GetObjectFactory();
            auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();

			const auto& compiledShader = ::Assets::GetAssetComp<CompiledShaderByteCode>(initializer);
            assert(compiledShader.GetStage() == ShaderStage::Geometry);
            auto byteCode = compiledShader.GetByteCode();
            underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
				byteCode.begin(), byteCode.size(),
                nativeDeclaration, delcCount,
                soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                    //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                    //                  we must disable the rasterization step some other way
                (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

        }

            //  (creation successful; we can commit to member now)
        _underlying = std::move(underlying);
    }

    GeometryShader::GeometryShader(const CompiledShaderByteCode& compiledShader, const StreamOutputInitializers& soInitializers)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Geometry);

            auto byteCode = compiledShader.GetByteCode();

            intrusive_ptr<ID3D::GeometryShader> underlying;
            if (soInitializers._outputBufferCount == 0) {

                underlying = GetObjectFactory().CreateGeometryShader(byteCode.begin(), byteCode.size());

            } else {

                assert(soInitializers._outputBufferCount <= D3D11_SO_BUFFER_SLOT_COUNT);
                D3D11_SO_DECLARATION_ENTRY nativeDeclaration[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];
                auto delcCount = BuildNativeDeclaration(nativeDeclaration, dimof(nativeDeclaration), soInitializers);

                auto& objFactory = GetObjectFactory();
                auto featureLevel = objFactory.GetUnderlying()->GetFeatureLevel();
                underlying = objFactory.CreateGeometryShaderWithStreamOutput( 
					byteCode.begin(), byteCode.size(),
                    nativeDeclaration, delcCount,
                    soInitializers._outputBufferStrides, soInitializers._outputBufferCount,
                        //      Note --     "NO_RASTERIZED_STREAM" is only supported on feature level 11. For other feature levels
                        //                  we must disable the rasterization step some other way
                    (featureLevel>=D3D_FEATURE_LEVEL_11_0)?D3D11_SO_NO_RASTERIZED_STREAM:0);

            }

            _underlying = std::move(underlying);
        }
    }

    GeometryShader::GeometryShader(GeometryShader&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
    }
    
    GeometryShader& GeometryShader::operator=(GeometryShader&& moveFrom)
    {
        _underlying = std::move(moveFrom._underlying);
        return *this;
    }

    GeometryShader::GeometryShader() {}
    GeometryShader::~GeometryShader() {}

    GeometryShader::StreamOutputInitializers GeometryShader::_hackInitializers;

    void GeometryShader::SetDefaultStreamOutputInitializers(const StreamOutputInitializers& initializers)
    {
        _hackInitializers = initializers;
    }

    auto GeometryShader::GetDefaultStreamOutputInitializers() -> const StreamOutputInitializers&
    {
        return _hackInitializers;
    }
#endif

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ComputeShader::ComputeShader(const CompiledShaderByteCode& compiledShader)
    {
        if (compiledShader.GetStage() != ShaderStage::Null) {
            assert(compiledShader.GetStage() == ShaderStage::Compute);
            auto byteCode = compiledShader.GetByteCode();
            _underlying = GetObjectFactory().CreateComputeShader(byteCode.begin(), byteCode.size());
        }

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, compiledShader.GetDependencyValidation());
    }

    ComputeShader::ComputeShader() {}

    ComputeShader::~ComputeShader() {}

        ////////////////////////////////////////////////////////////////////////////////////////////////

	void ShaderProgram::Apply(DeviceContext& devContext) const
	{
		ID3D::DeviceContext* d3dDevContext = devContext.GetUnderlying();
		d3dDevContext->VSSetShader(_vertexShader.get(), nullptr, 0);
		d3dDevContext->GSSetShader(_geometryShader.get(), nullptr, 0);
		d3dDevContext->PSSetShader(_pixelShader.get(), nullptr, 0);
		d3dDevContext->HSSetShader(_hullShader.get(), nullptr, 0);			// todo -- prevent frequent thrashing of this state
		d3dDevContext->DSSetShader(_domainShader.get(), nullptr, 0);
	}

	void ShaderProgram::Apply(DeviceContext& devContext, const BoundClassInterfaces& dynLinkage) const
	{
		ID3D::DeviceContext* d3dDevContext = devContext.GetUnderlying();
		auto& vsDyn = dynLinkage.GetClassInstances(ShaderStage::Vertex);
		auto& gsDyn = dynLinkage.GetClassInstances(ShaderStage::Geometry);
		auto& psDyn = dynLinkage.GetClassInstances(ShaderStage::Pixel);
		auto& hsDyn = dynLinkage.GetClassInstances(ShaderStage::Hull);
		auto& dsDyn = dynLinkage.GetClassInstances(ShaderStage::Domain);

		d3dDevContext->VSSetShader(_vertexShader.get(), (ID3D::ClassInstance*const*)AsPointer(vsDyn.cbegin()), (unsigned)vsDyn.size());
		d3dDevContext->GSSetShader(_geometryShader.get(), (ID3D::ClassInstance*const*)AsPointer(gsDyn.cbegin()), (unsigned)gsDyn.size());
		d3dDevContext->PSSetShader(_pixelShader.get(), (ID3D::ClassInstance*const*)AsPointer(psDyn.cbegin()), (unsigned)psDyn.size());
		d3dDevContext->HSSetShader(_hullShader.get(), (ID3D::ClassInstance*const*)AsPointer(hsDyn.cbegin()), (unsigned)hsDyn.size());
		d3dDevContext->DSSetShader(_domainShader.get(), (ID3D::ClassInstance*const*)AsPointer(dsDyn.cbegin()), (unsigned)dsDyn.size());
	}

    ShaderProgram::ShaderProgram(	ObjectFactory& factory, 
									const CompiledShaderByteCode& vs,
									const CompiledShaderByteCode& ps)
    {
		///////////////// VS ////////////////////
		if (vs.DynamicLinkingEnabled())
			_classLinkage[(unsigned)ShaderStage::Vertex] = factory.CreateClassLinkage();
			
		if (vs.GetStage() != ShaderStage::Null) {
			assert(vs.GetStage() == ShaderStage::Vertex);
			auto byteCode = vs.GetByteCode();
			_vertexShader = factory.CreateVertexShader(byteCode.begin(), byteCode.size(), _classLinkage[(unsigned)ShaderStage::Vertex].get());
			_compiledCode[(unsigned)ShaderStage::Vertex] = vs;
		}

		///////////////// PS ////////////////////
		if (ps.DynamicLinkingEnabled())
			_classLinkage[(unsigned)ShaderStage::Pixel] = factory.CreateClassLinkage();

		if (ps.GetStage() != ShaderStage::Null) {
			assert(ps.GetStage() == ShaderStage::Pixel);
			auto byteCode = ps.GetByteCode();
			_pixelShader = factory.CreatePixelShader(byteCode.begin(), byteCode.size(), _classLinkage[(unsigned)ShaderStage::Pixel].get());
			_compiledCode[(unsigned)ShaderStage::Pixel] = ps;
		}

        _validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterAssetDependency(_validationCallback, vs.GetDependencyValidation());
        Assets::RegisterAssetDependency(_validationCallback, ps.GetDependencyValidation());
    }
    
    ShaderProgram::ShaderProgram(   ObjectFactory& factory, 
									const CompiledShaderByteCode& vs,
									const CompiledShaderByteCode& gs,
									const CompiledShaderByteCode& ps)
	: ShaderProgram(factory, vs, ps)
    {
		///////////////// GS ////////////////////
		if (gs.DynamicLinkingEnabled())
			_classLinkage[(unsigned)ShaderStage::Geometry] = factory.CreateClassLinkage();

		if (gs.GetStage() != ShaderStage::Null) {
			assert(gs.GetStage() == ShaderStage::Geometry);
			auto byteCode = gs.GetByteCode();
			_geometryShader = factory.CreateGeometryShader(byteCode.begin(), byteCode.size(), _classLinkage[(unsigned)ShaderStage::Geometry].get());
			_compiledCode[(unsigned)ShaderStage::Geometry] = gs;
		}

		Assets::RegisterAssetDependency(_validationCallback, gs.GetDependencyValidation());
    }

    ShaderProgram::ShaderProgram(	ObjectFactory& factory,
									const CompiledShaderByteCode& vs,
									const CompiledShaderByteCode& gs,
									const CompiledShaderByteCode& ps,
									const CompiledShaderByteCode& hs,
									const CompiledShaderByteCode& ds)
	: ShaderProgram(factory, vs, gs, ps)
    {
		///////////////// HS ////////////////////
		if (hs.DynamicLinkingEnabled())
			_classLinkage[(unsigned)ShaderStage::Hull] = factory.CreateClassLinkage();

		if (hs.GetStage() != ShaderStage::Null) {
			assert(hs.GetStage() == ShaderStage::Hull);
			auto byteCode = hs.GetByteCode();
			_hullShader = factory.CreateHullShader(byteCode.begin(), byteCode.size(), _classLinkage[(unsigned)ShaderStage::Hull].get());
			_compiledCode[(unsigned)ShaderStage::Hull] = hs;
		}

		///////////////// DS ////////////////////
		if (ds.DynamicLinkingEnabled())
			_classLinkage[(unsigned)ShaderStage::Domain] = factory.CreateClassLinkage();

		if (ds.GetStage() != ShaderStage::Null) {
			assert(ds.GetStage() == ShaderStage::Domain);
			auto byteCode = ds.GetByteCode();
			_domainShader = factory.CreateDomainShader(byteCode.begin(), byteCode.size(), _classLinkage[(unsigned)ShaderStage::Domain].get());
			_compiledCode[(unsigned)ShaderStage::Domain] = ds;
		}

		Assets::RegisterAssetDependency(_validationCallback, hs.GetDependencyValidation());
		Assets::RegisterAssetDependency(_validationCallback, ds.GetDependencyValidation());
    }

	ShaderProgram::ShaderProgram() {}
    ShaderProgram::~ShaderProgram() {}

    bool ShaderProgram::DynamicLinkingEnabled() const
    {
        for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c)
			if (_classLinkage[c]) return true;
		return false;
    }

	ShaderProgram::ShaderProgram(ShaderProgram&& moveFrom) never_throws
	: _vertexShader(std::move(moveFrom._vertexShader))
	, _pixelShader(std::move(moveFrom._pixelShader))
	, _geometryShader(std::move(moveFrom._geometryShader))
	, _hullShader(std::move(moveFrom._hullShader))
	, _domainShader(std::move(moveFrom._domainShader))
	, _validationCallback(std::move(moveFrom._validationCallback))
	{
		for (unsigned c=0; c<(unsigned)ShaderStage::Max; ++c) {
			_compiledCode[c] = std::move(moveFrom._compiledCode[c]);
			_classLinkage[c] = std::move(moveFrom._classLinkage[c]);
		}
	}

    ShaderProgram& ShaderProgram::operator=(ShaderProgram&& moveFrom) never_throws
	{
		_vertexShader = std::move(moveFrom._vertexShader);
		_pixelShader = std::move(moveFrom._pixelShader);
		_geometryShader = std::move(moveFrom._geometryShader);
		_hullShader = std::move(moveFrom._hullShader);
		_domainShader = std::move(moveFrom._domainShader);
		_validationCallback = std::move(moveFrom._validationCallback);

		for (unsigned c = 0; c<(unsigned)ShaderStage::Max; ++c) {
			_compiledCode[c] = std::move(moveFrom._compiledCode[c]);
			_classLinkage[c] = std::move(moveFrom._classLinkage[c]);
		}
		return *this;
	}

    template intrusive_ptr<ID3D::ShaderReflection>;
}}

