// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonUtils.h"
#include "PipelineAccelerator.h"
#include "CompiledShaderPatchCollection.h"
#include "DescriptorSetAccelerator.h"
#include "../Assets/Services.h"
#include "../Assets/MaterialScaffold.h"
#include "../Assets/PredefinedDescriptorSetLayout.h"
#include "../IDevice.h"
#include "../ResourceDesc.h"
#include "../Types.h"
#include "../Metal/ObjectFactory.h"
#include "../Metal/Shader.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/Assets.h"
#include <sstream>

namespace RenderCore { namespace Techniques
{

	RenderCore::IResourcePtr CreateStaticVertexBuffer(IteratorRange<const void*> data)
	{
		return CreateStaticVertexBuffer(RenderCore::Assets::Services::GetDevice(), data);
	}

	RenderCore::IResourcePtr CreateStaticIndexBuffer(IteratorRange<const void*> data)
	{
		return CreateStaticIndexBuffer(RenderCore::Assets::Services::GetDevice(), data);
	}

	RenderCore::IResourcePtr CreateStaticVertexBuffer(IDevice& device, IteratorRange<const void*> data)
	{
		return device.CreateResource(
			CreateDesc(
				BindFlag::VertexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"vb"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}

	RenderCore::IResourcePtr CreateStaticIndexBuffer(IDevice& device, IteratorRange<const void*> data)
	{
		return device.CreateResource(
			CreateDesc(
				BindFlag::IndexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"ib"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}

	static void TryRegisterDependency(
		::Assets::DepValPtr& dst,
		const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
	{
		auto futureDepVal = future->GetDependencyValidation();
		if (futureDepVal)
			::Assets::RegisterAssetDependency(dst, futureDepVal);
	}

	::Assets::FuturePtr<Metal::ShaderProgram> CreateShaderProgramFromByteCode(
		const ::Assets::FuturePtr<CompiledShaderByteCode>& vsCode,
		const ::Assets::FuturePtr<CompiledShaderByteCode>& psCode,
		const std::string& programName)
	{
		assert(vsCode && psCode);
		auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>(programName);
		future->SetPollingFunction(
			[vsCode, psCode](::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>& thatFuture) -> bool {

				auto vsActual = vsCode->TryActualize();
				auto psActual = psCode->TryActualize();

				if (!vsActual || !psActual) {
					auto vsState = vsCode->GetAssetState();
					auto psState = psCode->GetAssetState();
					if (vsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
						auto depVal = std::make_shared<::Assets::DependencyValidation>();
						TryRegisterDependency(depVal, vsCode);
						TryRegisterDependency(depVal, psCode);
						std::stringstream log;
						if (vsState == ::Assets::AssetState::Invalid) log << "Vertex shader is invalid with message: " << std::endl << ::Assets::AsString(vsCode->GetActualizationLog()) << std::endl;
						if (psState == ::Assets::AssetState::Invalid) log << "Pixel shader is invalid with message: " << std::endl << ::Assets::AsString(psCode->GetActualizationLog()) << std::endl;
						thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob(log.str()));
						return false;
					}
					return true;
				}

				auto newShaderProgram = std::make_shared<RenderCore::Metal::ShaderProgram>(
					RenderCore::Metal::GetObjectFactory(), *vsActual, *psActual);
				thatFuture.SetAsset(std::move(newShaderProgram), {});
				return false;
			});
		return future;
	}

	::Assets::FuturePtr<Metal::ShaderProgram> CreateShaderProgramFromByteCode(
		const ::Assets::FuturePtr<CompiledShaderByteCode>& vsCode,
		const ::Assets::FuturePtr<CompiledShaderByteCode>& gsCode,
		const ::Assets::FuturePtr<CompiledShaderByteCode>& psCode,
		const StreamOutputInitializers& soInit,
		const std::string& programName)
	{
		assert(vsCode && psCode && gsCode);
		std::vector<RenderCore::InputElementDesc> soElements { soInit._outputElements.begin(), soInit._outputElements.end() };
		std::vector<unsigned> soStrides { soInit._outputBufferStrides.begin(), soInit._outputBufferStrides.end() };
		auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>(programName);
		future->SetPollingFunction(
			[vsCode, gsCode, psCode, soElements, soStrides](::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>& thatFuture) -> bool {

				auto vsActual = vsCode->TryActualize();
				auto gsActual = gsCode->TryActualize();
				auto psActual = psCode->TryActualize();

				if (!vsActual || !gsActual || !psActual) {
					auto vsState = vsCode->GetAssetState();
					auto gsState = gsCode->GetAssetState();
					auto psState = psCode->GetAssetState();
					if (vsState == ::Assets::AssetState::Invalid || gsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
						auto depVal = std::make_shared<::Assets::DependencyValidation>();
						TryRegisterDependency(depVal, vsCode);
						TryRegisterDependency(depVal, gsCode);
						TryRegisterDependency(depVal, psCode);
						std::stringstream log;
						if (vsState == ::Assets::AssetState::Invalid) log << "Vertex shader is invalid with message: " << std::endl << ::Assets::AsString(vsCode->GetActualizationLog()) << std::endl;
						if (gsState == ::Assets::AssetState::Invalid) log << "Geometry shader is invalid with message: " << std::endl << ::Assets::AsString(gsCode->GetActualizationLog()) << std::endl;
						if (psState == ::Assets::AssetState::Invalid) log << "Pixel shader is invalid with message: " << std::endl << ::Assets::AsString(psCode->GetActualizationLog()) << std::endl;
						thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob(log.str()));
						return false;
					}
					return true;
				}

				StreamOutputInitializers soInit;
				soInit._outputElements = MakeIteratorRange(soElements);
				soInit._outputBufferStrides = MakeIteratorRange(soStrides);

				auto newShaderProgram = std::make_shared<RenderCore::Metal::ShaderProgram>(
					RenderCore::Metal::GetObjectFactory(), *vsActual, *gsActual, *psActual, soInit);
				thatFuture.SetAsset(std::move(newShaderProgram), {});
				return false;
			});
		return future;
	}

	const RenderCore::Assets::PredefinedDescriptorSetLayout& GetFallbackMaterialDescriptorSetLayout()
	{
		return ::Assets::GetAsset<RenderCore::Assets::PredefinedDescriptorSetLayout>("xleres/Techniques/IllumLegacy.ds");
	}

	std::pair<std::shared_ptr<PipelineAccelerator>, std::shared_ptr<DescriptorSetAccelerator>>
		CreatePipelineAccelerator(
			PipelineAcceleratorPool& pool,
			const std::shared_ptr<CompiledShaderPatchCollection>& patchCollection,
			const RenderCore::Assets::MaterialScaffoldMaterial& material,
			IteratorRange<const RenderCore::InputElementDesc*> inputLayout,
			Topology topology)
	{
		std::shared_ptr<DescriptorSetAccelerator> descriptorSetAccelerator;

		auto matSelectors = material._matParams;

		if (patchCollection) {
			const auto* descriptorSetLayout = patchCollection->GetInterface().GetMaterialDescriptorSet().get();
			if (!descriptorSetLayout) {
				descriptorSetLayout = &RenderCore::Techniques::GetFallbackMaterialDescriptorSetLayout();
			}
			auto descriptorSetFuture = RenderCore::Techniques::MakeDescriptorSetAccelerator(
				material._constants, material._bindings,
				*descriptorSetLayout,
				"MaterialVisualizationScene");
			descriptorSetFuture->StallWhilePending();		// note the stall here to wait for resources
			descriptorSetAccelerator = descriptorSetFuture->Actualize();
			
			// Also append the "RES_HAS_" constants for each resource that is both in the descriptor set and that we have a binding for
			for (const auto&r:descriptorSetLayout->_resources)
				if (material._bindings.HasParameter(MakeStringSection(r._name)))
					matSelectors.SetParameter(MakeStringSection(std::string{"RES_HAS_"} + r._name).Cast<utf8>(), 1);
		}

		auto pipelineAccelerator = pool.CreatePipelineAccelerator(
			patchCollection,
			matSelectors,
			inputLayout,
			topology,
			material._stateSet);

		return std::make_pair(pipelineAccelerator, descriptorSetAccelerator);
	}

}}

