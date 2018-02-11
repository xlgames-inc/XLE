// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBuffer.h"
#include "Format.h"
#include "Resource.h"
#include "TextureView.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../ResourceUtils.h"
#include "../../Format.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    static VkAttachmentLoadOp AsLoadOp(LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case LoadStore::DontCare: 
        case LoadStore::DontCare_RetainStencil: 
        case LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case LoadStore::Retain: 
        case LoadStore::Retain_RetainStencil: 
        case LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadStore::Clear: 
        case LoadStore::Clear_RetainStencil: 
        case LoadStore::Clear_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
    }

    static VkAttachmentStoreOp AsStoreOp(LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case LoadStore::Clear: 
        case LoadStore::Clear_RetainStencil: 
        case LoadStore::Clear_ClearStencil: 
        case LoadStore::DontCare: 
        case LoadStore::DontCare_RetainStencil: 
        case LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case LoadStore::Retain: 
        case LoadStore::Retain_RetainStencil: 
        case LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    static VkAttachmentLoadOp AsLoadOpStencil(LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case LoadStore::Clear: 
        case LoadStore::DontCare: 
        case LoadStore::Retain: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        case LoadStore::Clear_ClearStencil: 
        case LoadStore::DontCare_ClearStencil: 
        case LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;

        case LoadStore::Clear_RetainStencil: 
        case LoadStore::DontCare_RetainStencil: 
        case LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
    }

    static VkAttachmentStoreOp AsStoreOpStencil(LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case LoadStore::Clear: 
        case LoadStore::DontCare: 
        case LoadStore::Retain: 
        case LoadStore::Clear_ClearStencil: 
        case LoadStore::DontCare_ClearStencil: 
        case LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;

        case LoadStore::Clear_RetainStencil: 
        case LoadStore::DontCare_RetainStencil: 
        case LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

	namespace Internal
	{
		namespace AttachmentUsageType
		{
			enum Flags
			{
				Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
			};
			using BitField = unsigned;
		}
	}

	static VkImageLayout AsShaderReadLayout(unsigned attachmentUsage)
	{
		if (attachmentUsage & unsigned(Internal::AttachmentUsageType::DepthStencil))
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

#if 0
	static VkImageLayout AsShaderReadLayout(const AttachmentDesc& desc)
    {
        if (desc._flags & AttachmentDesc::Flags::DepthStencil)
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    static unsigned FindIndex(IteratorRange<const AttachmentViewDesc*> attachments, AttachmentName name)
    {
        auto i = std::find_if(
            attachments.begin(), attachments.end(), 
            [name](const AttachmentViewDesc& desc) { return desc._resourceName == name; });
        if (i != attachments.end())
            return (unsigned)std::distance(attachments.begin(), i);
        return ~0u;
    }

    struct SubpassDep
    {
        unsigned        _srcSubpassIndex;
        AttachmentName  _attachment;
        VkAccessFlags   _srcAccessFlags;
        VkAccessFlags   _dstAccessFlags;

        static bool Compare(const SubpassDep& lhs, const SubpassDep& rhs)
        {
            if (lhs._srcSubpassIndex < rhs._srcSubpassIndex) return true;
            if (lhs._srcSubpassIndex > rhs._srcSubpassIndex) return false;
            if (lhs._attachment < rhs._attachment) return true;
            if (lhs._attachment > rhs._attachment) return false;
            return lhs._srcAccessFlags < rhs._srcAccessFlags;
        }
    };

    static SubpassDep FindWriter(IteratorRange<const SubpassDesc*> subpasses, AttachmentName attachment)
    {
        // Here, we return VK_SUBPASS_EXTERNAL when we cannot find the attachment (meaning that it might be written
        // by something outside of the render pass)
        if (subpasses.empty()) return { VK_SUBPASS_EXTERNAL, attachment, 0, 0 };
        for (int i=int(subpasses.size()-1); i>=0; --i) {
            auto& p = subpasses[i];
            // Is "attachment" listed amongst the output attachments?
            for (auto q:p._output) {
                if (q == attachment) return SubpassDep { (unsigned)i, attachment, (VkAccessFlags)VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 };
            }
            for (auto q:p._resolve) {
                if (q == attachment) return { (unsigned)i, attachment, (VkAccessFlags)VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 };
            }
            if (p._depthStencil == attachment) return { (unsigned)i, attachment, (VkAccessFlags)VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0 };
        }
        return { VK_SUBPASS_EXTERNAL, attachment, 0 };
    }

    static bool IsRetained(LoadStore loadStore)
    {
        return  loadStore == LoadStore::Retain
            ||  loadStore == LoadStore::Retain_RetainStencil
            ||  loadStore == LoadStore::Retain_ClearStencil
            ||  loadStore == LoadStore::DontCare_RetainStencil
            ||  loadStore == LoadStore::Clear_RetainStencil;
    }

    static void BuildOutputDeps(
        std::vector<SubpassDep>& result,
        IteratorRange<const AttachmentName*> attachmentsToTest,
        IteratorRange<const AttachmentViewDesc*> attachmentViews,
        IteratorRange<const AttachmentDesc*> attachmentResources,
        VkAccessFlags srcAccessFlags)
    {
        for (auto ai:attachmentsToTest) {
            auto a = std::find_if(
                attachmentViews.begin(), attachmentViews.end(),
                [ai](const AttachmentViewDesc& adesc) { return adesc._resourceName == ai; });
            if (a == attachmentViews.end()) { assert(0); continue; }   // couldn't find it?

            bool isRetainStore = IsRetained(a->_storeToNextPhase);
            if (!isRetainStore) continue;

            auto res = Find(attachmentResources, a->_resourceName);
            assert(res);

            VkAccessFlags dstAccessFlags = 0;
            if (res->_flags & AttachmentDesc::Flags::ShaderResource)
                dstAccessFlags |= VK_ACCESS_SHADER_READ_BIT;
            if (res->_flags & AttachmentDesc::Flags::TransferSource)
                dstAccessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
            if (!dstAccessFlags) continue;

            result.push_back(SubpassDep{0, ai, srcAccessFlags, dstAccessFlags});
        }
    }

    static std::vector<VkSubpassDependency> CalculateDependencies(const FrameBufferDesc& layout)
    {
        const auto& subpasses = layout.GetSubpasses();

        if (subpasses.empty())
            return std::vector<VkSubpassDependency>();

        // We must manually go through and detect dependencies between sub passes.
        // For every input attachment, there must be a dependency on the last subpass that wrote to it.
        // For an output attachment do we also need a dependency on the last subpass that reads from it or writes to it?
        // Let's ignore cases like 2 separate subpasses writing to the same attachment, that is later read by another subpass
        std::vector<VkSubpassDependency> result;
        for (unsigned c=0;c<unsigned(subpasses.size()); ++c) {
            // Check each input dependency, and look for previous subpasses that wrote to it
            // We also need to do this for color and depthstencil attachments
            std::vector<SubpassDep> dependencies;
            const auto& subpass = subpasses[c];
            for (const auto& a:subpass._input)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a._resourceName));
            for (const auto& a:subpass._output)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a._resourceName));
            if (subpass._depthStencil._resourceName != SubpassDesc::Unused._resourceName)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), subpass._depthStencil._resourceName));

            std::sort(dependencies.begin(), dependencies.end(), SubpassDep::Compare);
            auto newEnd = std::unique(dependencies.begin(), dependencies.end(), SubpassDep::Compare);
            
            // for each of these dependencies, create a VkSubpassDependency
            for (auto d=dependencies.begin(); d!=newEnd; ++d) {
                // note --  we actually need to know the last write to the resource in
                //          order to calculate "srcStageMask". We might need to add some flags
                //          in the input to handle this case for external dependencies.
                auto srcAccessFlags = d->_srcAccessFlags;
                if (!srcAccessFlags) {
                    // find the particular attachment and look for flags
                    auto a = std::find_if(
                        attachments.begin(), attachments.end(),
                        [d](const AttachmentViewDesc& adesc) { return adesc._resourceName == d->_attachment; });
                    if (a != attachments.end()) {
                        auto res = Find(attachmentResources, a->_resourceName);
                        assert(res);
                        srcAccessFlags = 
                            (res->_flags & AttachmentDesc::Flags::DepthStencil)
                            ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    } else 
                        srcAccessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // (just assume color attachment write)
                }

                result.push_back(
                    VkSubpassDependency {
                        d->_srcSubpassIndex, c, 
                        d->_srcAccessFlags, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0});
            }
        }

        // We must also generate "output" dependencies for any resources that are written or preserved by the final subpass
        // this is only required for resources that might be used by future render passes (or in some other fashion)
        {
            std::vector<SubpassDep> outputDeps;
            const auto& finalSubpass = subpasses[subpasses.size()-1];
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._output), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._preserve), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._resolve), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            if (finalSubpass._depthStencil != SubpassDesc::Unused)
                BuildOutputDeps(
                    outputDeps, 
                    MakeIteratorRange(&finalSubpass._depthStencil, &finalSubpass._depthStencil + 1),
                    attachments, 
                    attachmentResources, 
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        
            std::sort(outputDeps.begin(), outputDeps.end(), SubpassDep::Compare);
            auto newEnd = std::unique(outputDeps.begin(), outputDeps.end(), SubpassDep::Compare);
            
            // for each of these dependencies, create a VkSubpassDependency
            
            for (auto d=outputDeps.begin(); d!=newEnd; ++d)
                result.push_back(
                    VkSubpassDependency {
                        d->_srcSubpassIndex, VK_SUBPASS_EXTERNAL, 
                        d->_srcAccessFlags, d->_dstAccessFlags,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0});
        }

		// Also create subpass dependencies for every subpass. This is required currently, because we can sometimes
		// use vkCmdPipelineBarrier with a global memory barrier to push through dynamic constants data. However, this
		// might defeat some of the key goals of the render pass system!
		for (unsigned c = 0; c<unsigned(subpasses.size()); ++c) {
			result.push_back(VkSubpassDependency{
				c, c, 
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_ACCESS_HOST_WRITE_BIT,
				VK_ACCESS_INDIRECT_COMMAND_READ_BIT
				| VK_ACCESS_INDEX_READ_BIT
				| VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
				| VK_ACCESS_UNIFORM_READ_BIT
				| VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
				| VK_ACCESS_SHADER_READ_BIT,
				0});
		}

        return result;
    }

    namespace Internal
    {
        static unsigned GetAttachmentUsage(const FrameBufferDesc& layout, AttachmentName attachment)
        {
            unsigned result = 0u;
            for (auto& s:layout.GetSubpasses()) {
                auto i = std::find_if(
					s._input.begin(), s._input.end(), 
					[attachment](const AttachmentViewDesc& vd) { return vd._resourceName == attachment; });
                if (i != s._input.end()) 
                    result |= unsigned(Internal::AttachmentUsage::Input);
    
                auto o = std::find_if(
					s._output.begin(), s._output.end(), 
					[attachment](const AttachmentViewDesc& vd) { return vd._resourceName == attachment; });
                if (o != s._output.end()) 
                    result |= unsigned(Internal::AttachmentUsage::Output);
    
                if (s._depthStencil._resourceName == attachment)
                    result |= unsigned(Internal::AttachmentUsage::DepthStencil);
            }
            return result;
        }
    }

    VulkanUniquePtr<VkRenderPass> CreateRenderPass(
        const Metal_Vulkan::ObjectFactory& factory,
        const FrameBufferDesc& layout,
        IteratorRange<const AttachmentDesc*> attachmentResources,
        TextureSamples samples)
    {
        auto subpasses = layout.GetSubpasses();

        std::vector<VkAttachmentDescription> attachmentDesc;
        attachmentDesc.reserve(attachments.size());
        for (auto&a:attachments) {
            const auto* resourceDesc = Find(attachmentResources, a._resourceName);
            assert(resourceDesc);

            auto formatFilter = a._window._format;
            if (formatFilter._aspect == TextureViewDesc::UndefinedAspect)
                formatFilter._aspect = resourceDesc->_defaultAspect;
            FormatUsage formatUsage = FormatUsage::SRV;
            auto attachUsage = Internal::GetAttachmentUsage(layout, a._viewName);
            if (attachUsage & unsigned(Internal::AttachmentUsage::Output)) formatUsage = FormatUsage::RTV;
            if (attachUsage & unsigned(Internal::AttachmentUsage::DepthStencil)) formatUsage = FormatUsage::DSV;
            auto resolvedFormat = ResolveFormat(resourceDesc->_format, formatFilter, formatUsage);

            VkAttachmentDescription desc;
            desc.flags = 0;
            desc.format = AsVkFormat(resolvedFormat);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = AsLoadOp(a._loadFromPreviousPhase);
            desc.storeOp = AsStoreOp(a._storeToNextPhase);
            desc.stencilLoadOp = AsLoadOpStencil(a._loadFromPreviousPhase);
            desc.stencilStoreOp = AsStoreOpStencil(a._storeToNextPhase);

            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool isDepthStencil = !!(resourceDesc->_flags & AttachmentDesc::Flags::DepthStencil);
            if (isDepthStencil) {
                desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            // Assume that that attachments marked "ShaderResource" will begin and end in the 
            // shader read layout. This should be fine for cases where a single render pass
            // is used to write to a texture, and then it is read subsequentially. 
            //
            // However, if there are multiple writing render passes, followed by a shader
            // read at some later point, then this may switch to shader read layout redundantly
            if (resourceDesc->_flags & AttachmentDesc::Flags::ShaderResource) {
                desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            // desc.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (a._resourceName == 0u) {
                // we assume that name "0" is always bound to a presentable buffer
                assert(!isDepthStencil);
                desc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            } 
            
            if (resourceDesc->_flags & AttachmentDesc::Flags::Multisampled)
                desc.samples = AsSampleCountFlagBits(samples);

            // note --  do we need to set VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL or 
            //          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL as appropriate for input attachments
            //          (we should be able to tell this from the subpasses)?

            attachmentDesc.push_back(desc);
        }

        std::vector<VkAttachmentReference> attachReferences;
        std::vector<uint32_t> preserveAttachments;

        std::vector<VkSubpassDescription> subpassDesc;
        subpassDesc.reserve(subpasses.size());
        for (auto&p:subpasses) {
            VkSubpassDescription desc;
            desc.flags = 0;
            desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            // Input attachments are going to be difficult, because they must be bound both
            // by the sub passes and by the descriptor set! (and they must be explicitly listed as
            // input attachments in the shader). Holy cow, the render pass, frame buffer, pipeline
            // layout, descriptor set and shader must all agree!
            auto beforeInputs = attachReferences.size();
            for (auto& a:p._input) {
                // presumably we want to shader the shader read only layout modes in these cases.
                if (a != SubpassDesc::Unused) {
                    auto attachmentIndex = FindIndex(attachments, a);
                    assert(attachmentIndex < attachmentDesc.size());
                    auto layout2 = AsShaderReadLayout(*Find(attachmentResources, attachments[attachmentIndex]._resourceName));
                    attachReferences.push_back(VkAttachmentReference{attachmentIndex, layout2});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pInputAttachments = (const VkAttachmentReference*)(beforeInputs+1);
            desc.inputAttachmentCount = uint32_t(attachReferences.size() - beforeInputs);

            auto beforeOutputs = attachReferences.size();
            for (auto& a:p._output) {
                if (a != SubpassDesc::Unused) {
                    auto attachmentIndex = FindIndex(attachments, a);
                    assert(attachmentIndex < attachmentDesc.size());
                    auto layout2 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;        // basically should be VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL, I guess?
                    attachReferences.push_back(VkAttachmentReference{attachmentIndex, layout2});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pColorAttachments = (const VkAttachmentReference*)(beforeOutputs+1);
            desc.colorAttachmentCount = uint32_t(attachReferences.size() - beforeOutputs);
            desc.pResolveAttachments = nullptr; // not supported

            if (p._depthStencil != SubpassDesc::Unused) {
                auto attachmentIndex = FindIndex(attachments, p._depthStencil);
                desc.pDepthStencilAttachment = (const VkAttachmentReference*)(attachReferences.size()+1);
                attachReferences.push_back(VkAttachmentReference{attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
            } else {
                desc.pDepthStencilAttachment = nullptr;
            }

            auto beforePreserve = preserveAttachments.size();
            for (auto&a:p._preserve) {
                auto attachmentIndex = FindIndex(attachments, a);
                assert(attachmentIndex < attachmentDesc.size());
                preserveAttachments.push_back(attachmentIndex);
            }
            desc.pPreserveAttachments = (const uint32_t*)(beforePreserve+1);
            desc.preserveAttachmentCount = uint32_t(preserveAttachments.size() - beforePreserve);

            if (!p._resolve.empty()) {
                assert(p._resolve.size() == p._output.size());
                auto beforeResolve = attachReferences.size();
                for (auto&a:p._resolve) {
                    auto attachmentIndex = FindIndex(attachments, a);
                    assert(attachmentIndex < attachmentDesc.size());
                    attachReferences.push_back(VkAttachmentReference{attachmentIndex, attachmentDesc[attachmentIndex].finalLayout});
                }
                desc.pResolveAttachments = (const VkAttachmentReference*)(beforeResolve+1);
            } else {
                desc.pResolveAttachments = nullptr;
            }

            subpassDesc.push_back(desc);
        }

        // we need to do a fixup pass over all of the subpasses to generate correct pointers
        for (auto&p:subpassDesc) {
            if (p.pInputAttachments)
                p.pInputAttachments = AsPointer(attachReferences.begin()) + size_t(p.pInputAttachments)-1;
            if (p.pColorAttachments)
                p.pColorAttachments = AsPointer(attachReferences.begin()) + size_t(p.pColorAttachments)-1;
            if (p.pResolveAttachments)
                p.pResolveAttachments = AsPointer(attachReferences.begin()) + size_t(p.pResolveAttachments)-1;
            if (p.pDepthStencilAttachment)
                p.pDepthStencilAttachment = AsPointer(attachReferences.begin()) + size_t(p.pDepthStencilAttachment)-1;
            if (p.pPreserveAttachments)
                p.pPreserveAttachments = AsPointer(preserveAttachments.begin()) + size_t(p.pPreserveAttachments)-1;
        }

        auto dependencies = CalculateDependencies(layout, attachmentResources);

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = nullptr;
        rp_info.attachmentCount = (uint32_t)attachmentDesc.size();
        rp_info.pAttachments = AsPointer(attachmentDesc.begin());
        rp_info.subpassCount = (uint32_t)subpassDesc.size();
        rp_info.pSubpasses = AsPointer(subpassDesc.begin());
        rp_info.dependencyCount = (uint32_t)dependencies.size();
        rp_info.pDependencies = AsPointer(dependencies.begin());

        return factory.CreateRenderPass(rp_info);
    }
#endif

	/*
	class WorkingAttachments 
	{
	public:

	};

	std::vector<WorkingAttachments> CalculateWorkingAttachments(const FrameBufferDesc& layout)
	{
		
        
	}*/

	static bool HasRetain(LoadStore ls)
	{
		return ls == LoadStore::Retain
			|| ls == LoadStore::DontCare_RetainStencil
			|| ls == LoadStore::Retain_RetainStencil
			|| ls == LoadStore::Clear_RetainStencil
			|| ls == LoadStore::Retain_ClearStencil;
	}
	
	static void MergeFormatFilter(TextureViewDesc::FormatFilter& dst, TextureViewDesc::FormatFilter src)
	{
		assert(dst._aspect == src._aspect
			|| dst._aspect == TextureViewDesc::Aspect::UndefinedAspect
			|| src._aspect == TextureViewDesc::Aspect::UndefinedAspect);
		if (src._aspect != TextureViewDesc::Aspect::UndefinedAspect)
			dst._aspect = src._aspect;
	}

	VulkanUniquePtr<VkRenderPass> CreateRenderPass(
        const Metal_Vulkan::ObjectFactory& factory,
        const FrameBufferDesc& layout,
        const INamedAttachments& namedResources,
        TextureSamples samples)
	{
		const auto subpasses = layout.GetSubpasses();

		struct AttachmentUsage
		{
			unsigned _subpassIdx = ~0u;
			Internal::AttachmentUsageType::BitField _usage = 0;
			LoadStore _loadStore = LoadStore::DontCare;
		};
		struct WorkingAttachment
		{
			AttachmentUsage _lastSubpassWrite;
			AttachmentUsage _lastSubpassRead;
			TextureViewDesc::FormatFilter _formatFilter;
			Internal::AttachmentUsageType::BitField _attachmentUsage = 0;
		};
		std::vector<std::pair<AttachmentName, WorkingAttachment>> workingAttachments;
		workingAttachments.reserve(subpasses.size()*2);	// approximate

		struct SubpassDependency
		{
			AttachmentName _resource;
			AttachmentUsage _first;
			AttachmentUsage _second;
		};
		std::vector<SubpassDependency> dependencies;
		dependencies.reserve(subpasses.size()*2);	// approximate

		////////////////////////////////////////////////////////////////////////////////////
		// Build up the list of subpass dependencies and the set of unique attachments
        for (unsigned spIdx=0; spIdx<subpasses.size(); spIdx++) {
			const auto& spDesc = subpasses[spIdx];

			std::vector<std::pair<const AttachmentViewDesc*, Internal::AttachmentUsageType::BitField>> subpassAttachments;
			for (const auto& r:spDesc._output) 
				subpassAttachments.push_back({&r, Internal::AttachmentUsageType::Output});
			if (spDesc._depthStencil._resourceName != SubpassDesc::Unused._resourceName)
				subpassAttachments.push_back({&spDesc._depthStencil, Internal::AttachmentUsageType::DepthStencil});
			for (const auto& r:spDesc._input) 
				subpassAttachments.push_back({&r, Internal::AttachmentUsageType::Input});

			//////////////////////////////////////////////////////////////////////////////////////////

			for (const auto& spa:subpassAttachments) {
				const auto&r = *spa.first;
				auto resource = r._resourceName;
				auto i = LowerBound(workingAttachments, resource);
				if (i == workingAttachments.end() || i->first != resource)
					i = workingAttachments.insert(i, {resource, WorkingAttachment{}});

				AttachmentUsage loadUsage { spIdx, Internal::AttachmentUsageType::Output, r._loadFromPreviousPhase };
				AttachmentUsage storeUsage {};
				if (r._storeToNextPhase == LoadStore::Retain)
					storeUsage = { spIdx, Internal::AttachmentUsageType::Output, r._storeToNextPhase };

				// If we're loading data from a previous phase, we've got to find it in
				// the working attachments, and create a subpass dependency
				// Otherwise, if there are any previous contents, they 
				// will be destroyed.
				if (HasRetain(r._loadFromPreviousPhase))
					dependencies.push_back({resource, i->second._lastSubpassWrite, loadUsage});

				// We also need a dependency with the last subpass to read from this 
				// attachment. We can't write to it until the reading is finished
				if (spa.second & (Internal::AttachmentUsageType::Output | Internal::AttachmentUsageType::DepthStencil)) {
					if (i->second._lastSubpassRead._subpassIdx != ~0u)
						dependencies.push_back({resource, i->second._lastSubpassRead, loadUsage});

					i->second._lastSubpassWrite = storeUsage;
				} else {
					i->second._lastSubpassRead = loadUsage;

					// If the data isn't marked as "retained" after this read, we must clear out the 
					// last subpass write flags
					if (!HasRetain(r._storeToNextPhase))
						i->second._lastSubpassWrite = {};
				}

				MergeFormatFilter(i->second._formatFilter, r._window._format);
				i->second._attachmentUsage |= Internal::AttachmentUsageType::Output;
			}
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Build the VkAttachmentDescription objects
		std::vector<VkAttachmentDescription> attachmentDesc;
        attachmentDesc.reserve(workingAttachments.size());
        for (auto&a:workingAttachments) {
            const auto* resourceDesc = namedResources.GetDesc(a.first);
            assert(resourceDesc);

            auto formatFilter = a.second._formatFilter;
            if (formatFilter._aspect == TextureViewDesc::UndefinedAspect)
                formatFilter._aspect = resourceDesc->_defaultAspect;
            FormatUsage formatUsage = FormatUsage::SRV;
            if (a.second._attachmentUsage & Internal::AttachmentUsageType::Output) formatUsage = FormatUsage::RTV;
            if (a.second._attachmentUsage & Internal::AttachmentUsageType::DepthStencil) formatUsage = FormatUsage::DSV;
            auto resolvedFormat = ResolveFormat(resourceDesc->_format, formatFilter, formatUsage);

			// look through the subpass dependencies to find load operations including
			LoadStore originalLoad = LoadStore::DontCare;
			for (const auto& d:dependencies) {
				if (d._resource == a.first && d._first._subpassIdx == ~0u) {
					// this is a load from pre-renderpass
					assert(originalLoad == LoadStore::DontCare || originalLoad == d._first._loadStore);
					originalLoad = d._first._loadStore;
				}
			}
			LoadStore finalStore = a.second._lastSubpassWrite._loadStore;

            VkAttachmentDescription desc;
            desc.flags = 0;
            desc.format = AsVkFormat(resolvedFormat);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = AsLoadOp(originalLoad);
            desc.stencilLoadOp = AsLoadOpStencil(originalLoad);
			desc.storeOp = AsStoreOp(finalStore);
            desc.stencilStoreOp = AsStoreOpStencil(finalStore);

            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool isDepthStencil = !!(a.second._attachmentUsage & unsigned(Internal::AttachmentUsageType::DepthStencil));// !!(resourceDesc->_flags & AttachmentDesc::Flags::DepthStencil);
            if (isDepthStencil) {
                desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            // Assume that that attachments marked "ShaderResource" will begin and end in the 
            // shader read layout. This should be fine for cases where a single render pass
            // is used to write to a texture, and then it is read subsequentially. 
            //
            // However, if there are multiple writing render passes, followed by a shader
            // read at some later point, then this may switch to shader read layout redundantly
            if (resourceDesc->_flags & AttachmentDesc::Flags::ShaderResource) {
                desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            // desc.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (a.first == 0u) {
                // we assume that name "0" is always bound to a presentable buffer
                assert(!isDepthStencil);
                desc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            } 
            
            if (resourceDesc->_flags & AttachmentDesc::Flags::Multisampled)
                desc.samples = AsSampleCountFlagBits(samples);

            // note --  do we need to set VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL or 
            //          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL as appropriate for input attachments
            //          (we should be able to tell this from the subpasses)?

            attachmentDesc.push_back(desc);
        }


		////////////////////////////////////////////////////////////////////////////////////
		// Build the actual VkSubpassDescription objects

		std::vector<VkAttachmentReference> attachReferences;
        std::vector<uint32_t> preserveAttachments;

        std::vector<VkSubpassDescription> subpassDesc;
        subpassDesc.reserve(subpasses.size());
        for (auto&p:subpasses) {
            VkSubpassDescription desc;
            desc.flags = 0;
            desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            // Input attachments are going to be difficult, because they must be bound both
            // by the sub passes and by the descriptor set! (and they must be explicitly listed as
            // input attachments in the shader). Holy cow, the render pass, frame buffer, pipeline
            // layout, descriptor set and shader must all agree!
            auto beforeInputs = attachReferences.size();
            for (auto& a:p._input) {
				auto resource = a._resourceName;
				auto i = LowerBound(workingAttachments, resource);
				assert(i != workingAttachments.end() && i->first == resource);
				auto internalName = std::distance(workingAttachments.begin(), i);
				attachReferences.push_back(VkAttachmentReference{(uint32_t)internalName, AsShaderReadLayout(i->second._attachmentUsage)});
            }
            desc.pInputAttachments = (const VkAttachmentReference*)(beforeInputs+1);
            desc.inputAttachmentCount = uint32_t(attachReferences.size() - beforeInputs);

            auto beforeOutputs = attachReferences.size();
            for (auto& a:p._output) {
				auto resource = a._resourceName;
				auto i = LowerBound(workingAttachments, resource);
				assert(i != workingAttachments.end() && i->first == resource);
				auto internalName = std::distance(workingAttachments.begin(), i);
				attachReferences.push_back(VkAttachmentReference{(uint32_t)internalName, AsShaderReadLayout(i->second._attachmentUsage)});
            }
            desc.pColorAttachments = (const VkAttachmentReference*)(beforeOutputs+1);
            desc.colorAttachmentCount = uint32_t(attachReferences.size() - beforeOutputs);
            desc.pResolveAttachments = nullptr; // not supported
			desc.pPreserveAttachments = nullptr;
			desc.preserveAttachmentCount = 0;

            if (p._depthStencil._resourceName != SubpassDesc::Unused._resourceName) {
				auto resource = p._depthStencil._resourceName;
				auto i = LowerBound(workingAttachments, resource);
				assert(i != workingAttachments.end() && i->first == resource);
				auto internalName = std::distance(workingAttachments.begin(), i);
				desc.pDepthStencilAttachment = (const VkAttachmentReference*)(attachReferences.size()+1);
				attachReferences.push_back(VkAttachmentReference{(uint32_t)internalName, AsShaderReadLayout(i->second._attachmentUsage)});
            } else {
                desc.pDepthStencilAttachment = nullptr;
            }

			// preserve & resolve attachments not supported currently

            subpassDesc.push_back(desc);
        }

        // we need to do a fixup pass over all of the subpasses to generate correct pointers
        for (auto&p:subpassDesc) {
            if (p.pInputAttachments)
                p.pInputAttachments = AsPointer(attachReferences.begin()) + size_t(p.pInputAttachments)-1;
            if (p.pColorAttachments)
                p.pColorAttachments = AsPointer(attachReferences.begin()) + size_t(p.pColorAttachments)-1;
            if (p.pResolveAttachments)
                p.pResolveAttachments = AsPointer(attachReferences.begin()) + size_t(p.pResolveAttachments)-1;
            if (p.pDepthStencilAttachment)
                p.pDepthStencilAttachment = AsPointer(attachReferences.begin()) + size_t(p.pDepthStencilAttachment)-1;
            if (p.pPreserveAttachments)
                p.pPreserveAttachments = AsPointer(preserveAttachments.begin()) + size_t(p.pPreserveAttachments)-1;
        }

		////////////////////////////////////////////////////////////////////////////////////
		// Build the actual VkSubpassDependency objects

        std::vector<VkSubpassDependency> vkDeps;
        for (unsigned c=0;c<unsigned(subpasses.size()); ++c) {
			// Find the list of SubPassDependency objects where _second is this subpass. We'll
			// then find the unique list of subpasses referenced by _first, and generate the
			// Vulkan object from them.
			//
			// Note that there are implicit dependencies to "VK_SUBPASS_EXTERNAL" which are defined
			// with a standard form. We'll rely on those implicit dependencies, rather than 
			// explicitly creating them here.

			std::vector<SubpassDependency> terminatingDependencies;
			for (const auto& d:dependencies)
				if (d._second._subpassIdx == c && d._first._subpassIdx != ~0u)
					terminatingDependencies.push_back(d);

			std::vector<VkSubpassDependency> deps;
			for (const auto& d:terminatingDependencies) {
				auto i = std::find_if(
					deps.begin(), deps.end(),
					[&d](const VkSubpassDependency& vkd) { return vkd.srcSubpass == d._first._subpassIdx; });
				if (i == deps.end())
					i = deps.insert(deps.end(), {
						d._first._subpassIdx, c, 
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        0, 0,	// access flags set below
						0});

				if (d._first._usage | Internal::AttachmentUsageType::Output)
					i->srcStageMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				if (d._first._usage | Internal::AttachmentUsageType::DepthStencil)
					i->srcStageMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				if (d._first._usage | Internal::AttachmentUsageType::Input)
					i->srcStageMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

				if (d._second._usage | Internal::AttachmentUsageType::Output)
					i->dstStageMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				if (d._second._usage | Internal::AttachmentUsageType::DepthStencil)
					i->dstStageMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				if (d._second._usage | Internal::AttachmentUsageType::Input)
					i->dstStageMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			}

			vkDeps.insert(vkDeps.end(), deps.begin(), deps.end());

#if 0
            // Check each input dependency, and look for previous subpasses that wrote to it
            // We also need to do this for color and depthstencil attachments
            std::vector<SubpassDep> dependencies;
            const auto& subpass = subpasses[c];
            for (const auto& a:subpass._input)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a._resourceName));
            for (const auto& a:subpass._output)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a._resourceName));
            if (subpass._depthStencil._resourceName != SubpassDesc::Unused._resourceName)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), subpass._depthStencil._resourceName));

            std::sort(dependencies.begin(), dependencies.end(), SubpassDep::Compare);
            auto newEnd = std::unique(dependencies.begin(), dependencies.end(), SubpassDep::Compare);
            
            // for each of these dependencies, create a VkSubpassDependency
            for (auto d=dependencies.begin(); d!=newEnd; ++d) {
                // note --  we actually need to know the last write to the resource in
                //          order to calculate "srcStageMask". We might need to add some flags
                //          in the input to handle this case for external dependencies.
                auto srcAccessFlags = d->_srcAccessFlags;
                if (!srcAccessFlags) {
                    // find the particular attachment and look for flags
                    auto a = std::find_if(
                        attachments.begin(), attachments.end(),
                        [d](const AttachmentViewDesc& adesc) { return adesc._resourceName == d->_attachment; });
                    if (a != attachments.end()) {
                        auto res = Find(attachmentResources, a->_resourceName);
                        assert(res);
                        srcAccessFlags = 
                            (res->_flags & AttachmentDesc::Flags::DepthStencil)
                            ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    } else 
                        srcAccessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // (just assume color attachment write)
                }

                vkDeps.push_back(
                    VkSubpassDependency {
                        d->_srcSubpassIndex, c, 
                        d->_srcAccessFlags, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0});
            }
#endif
        }

#if 0
        // We must also generate "output" dependencies for any resources that are written or preserved by the final subpass
        // this is only required for resources that might be used by future render passes (or in some other fashion)
        {
            std::vector<SubpassDep> outputDeps;
            const auto& finalSubpass = subpasses[subpasses.size()-1];
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._output), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._preserve), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._resolve), attachments, attachmentResources, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            if (finalSubpass._depthStencil != SubpassDesc::Unused)
                BuildOutputDeps(
                    outputDeps, 
                    MakeIteratorRange(&finalSubpass._depthStencil, &finalSubpass._depthStencil + 1),
                    attachments, 
                    attachmentResources, 
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        
            std::sort(outputDeps.begin(), outputDeps.end(), SubpassDep::Compare);
            auto newEnd = std::unique(outputDeps.begin(), outputDeps.end(), SubpassDep::Compare);
            
            // for each of these dependencies, create a VkSubpassDependency
            
            for (auto d=outputDeps.begin(); d!=newEnd; ++d)
                vkDeps.push_back(
                    VkSubpassDependency {
                        d->_srcSubpassIndex, VK_SUBPASS_EXTERNAL, 
                        d->_srcAccessFlags, d->_dstAccessFlags,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0});
        }
#endif

		// Also create subpass dependencies for every subpass. This is required currently, because we can sometimes
		// use vkCmdPipelineBarrier with a global memory barrier to push through dynamic constants data. However, this
		// might defeat some of the key goals of the render pass system!
		for (unsigned c = 0; c<unsigned(subpasses.size()); ++c) {
			vkDeps.push_back(VkSubpassDependency{
				c, c, 
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_ACCESS_HOST_WRITE_BIT,
				VK_ACCESS_INDIRECT_COMMAND_READ_BIT
				| VK_ACCESS_INDEX_READ_BIT
				| VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
				| VK_ACCESS_UNIFORM_READ_BIT
				| VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
				| VK_ACCESS_SHADER_READ_BIT,
				0});
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Build the final render pass object

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = nullptr;
        rp_info.attachmentCount = (uint32_t)attachmentDesc.size();
        rp_info.pAttachments = attachmentDesc.data();
        rp_info.subpassCount = (uint32_t)subpassDesc.size();
        rp_info.pSubpasses = subpassDesc.data();
        rp_info.dependencyCount = (uint32_t)vkDeps.size();
        rp_info.pDependencies = vkDeps.data();

        return factory.CreateRenderPass(rp_info);
	}

	struct MaxDims
	{
		unsigned _width = 0, _height = 0;
		unsigned _layers = 0;
	};

	static void BuildMaxDims(MaxDims& result, AttachmentName attachment, const INamedAttachments& namedResources, const FrameBufferProperties& props)
	{
		// This is annoying -- we need to look for any resources with
        // array layers. Ideally all our resources should have the same
        // array layer count (if they don't, we just end up selecting the
        // largest value)
        auto* desc = namedResources.GetDesc(attachment);
        if (desc) {
            result._layers = std::max(result._layers, desc->_arrayLayerCount);
            unsigned resWidth = 0u, resHeight = 0u;
                
            if (desc->_dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
                resWidth = unsigned(desc->_width);
                resHeight = unsigned(desc->_height);
            } else {
                resWidth = unsigned(std::floor(props._outputWidth * desc->_width + 0.5f));
                resHeight = unsigned(std::floor(props._outputHeight * desc->_height + 0.5f));
            }

            result._width = std::max(result._width, resWidth);
            result._height = std::max(result._height, resHeight);
        }
	}

    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
        const FrameBufferProperties& props,
        VkRenderPass layout,
        const INamedAttachments& namedResources)
    : _layout(layout)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
		auto subpasses = fbDesc.GetSubpasses();

        MaxDims maxDims;

		ViewPool<RenderTargetView> rtvPool;
        ViewPool<DepthStencilView> dsvPool;
		ViewPool<ShaderResourceView> srvPool;
        VkImageView rawViews[16];
		unsigned rawViewCount = 0;

        for (unsigned c=0; c<(unsigned)subpasses.size(); ++c) {
			const auto& spDesc = subpasses[c];

			for (const auto& r:spDesc._output) {
				auto resource = namedResources.GetResource(r._resourceName);
				auto* rtv = rtvPool.GetView(resource, r._window);
				rawViews[rawViewCount++] = rtv->GetImageView();
				BuildMaxDims(maxDims, r._resourceName, namedResources, props);
			}

			if (spDesc._depthStencil._resourceName != SubpassDesc::Unused._resourceName) {
				auto resource = namedResources.GetResource(spDesc._depthStencil._resourceName);
				auto* dsv = dsvPool.GetView(resource, spDesc._depthStencil._window);
				rawViews[rawViewCount++] = dsv->GetImageView();
				BuildMaxDims(maxDims, spDesc._depthStencil._resourceName, namedResources, props);
			}

			for (const auto& r:spDesc._input) {
				// todo -- these srvs also need to be exposed to the caller, so they can be bound to
				// the shader during the subpass
				auto resource = namedResources.GetResource(r._resourceName);
				auto* srv = srvPool.GetView(resource, r._window);
				rawViews[rawViewCount++] = srv->GetImageView();
				BuildMaxDims(maxDims, r._resourceName, namedResources, props);
			}
        }

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = layout;
        fb_info.attachmentCount = rawViewCount;
        fb_info.pAttachments = rawViews;
        fb_info.width = maxDims._width;
        fb_info.height = maxDims._height;
        fb_info.layers = std::max(1u, maxDims._layers);
        _underlying = factory.CreateFramebuffer(fb_info);

        // todo --  do we need to create a "patch up" command buffer to assign the starting image layouts
        //          for all of the images we created?
    }

	FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues)
    {
        context.BeginRenderPass(
            frameBuffer, TextureSamples::Create(),
            {0u, 0u}, {props._outputWidth, props._outputHeight},
            clearValues);
    }

    void BeginNextSubpass(DeviceContext& context, FrameBuffer&)
    {
        context.CmdNextSubpass(VK_SUBPASS_CONTENTS_INLINE);
    }

    void EndRenderPass(DeviceContext& context)
    {
        context.EndRenderPass();
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferPool::Pimpl 
    {
    public:
        std::vector<std::pair<uint64, VulkanUniquePtr<VkRenderPass>>> _layouts;
    };

    std::shared_ptr<FrameBuffer> FrameBufferPool::BuildFrameBuffer(
		const ObjectFactory& factory,
        const FrameBufferDesc& desc,
        const FrameBufferProperties& props,
        const INamedAttachments& namedResources,
        uint64 hashName)
    {
        auto layout = BuildFrameBufferLayout(factory, desc, namedResources, props._samples);
        return std::make_shared<FrameBuffer>(factory, desc, props, layout, namedResources);
    }

    VkRenderPass FrameBufferPool::BuildFrameBufferLayout(
        const ObjectFactory& factory,
        const FrameBufferDesc& desc,
        const INamedAttachments& namedResources,
        const TextureSamples& samples)
    {
        auto hash = desc.GetHash();
        auto i = LowerBound(_pimpl->_layouts, hash);
        if (i != _pimpl->_layouts.end() && i->first == hash)
            return i->second.get();

        auto rp = CreateRenderPass(factory, desc, namedResources, samples);
        auto result = rp.get();
        _pimpl->_layouts.insert(i, std::make_pair(hash, std::move(rp)));
        return result;
    }

    FrameBufferPool::FrameBufferPool()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::~FrameBufferPool()
    {}

}}

