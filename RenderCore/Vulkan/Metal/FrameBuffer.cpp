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
#include "../../Format.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    static VkAttachmentLoadOp AsLoadOp(AttachmentViewDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentViewDesc::LoadStore::DontCare: 
        case AttachmentViewDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case AttachmentViewDesc::LoadStore::Retain: 
        case AttachmentViewDesc::LoadStore::Retain_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentViewDesc::LoadStore::Clear: 
        case AttachmentViewDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Clear_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
    }

    static VkAttachmentStoreOp AsStoreOp(AttachmentViewDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentViewDesc::LoadStore::Clear: 
        case AttachmentViewDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentViewDesc::LoadStore::DontCare: 
        case AttachmentViewDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case AttachmentViewDesc::LoadStore::Retain: 
        case AttachmentViewDesc::LoadStore::Retain_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    static VkAttachmentLoadOp AsLoadOpStencil(AttachmentViewDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentViewDesc::LoadStore::Clear: 
        case AttachmentViewDesc::LoadStore::DontCare: 
        case AttachmentViewDesc::LoadStore::Retain: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        case AttachmentViewDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_ClearStencil: 
        case AttachmentViewDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;

        case AttachmentViewDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
    }

    static VkAttachmentStoreOp AsStoreOpStencil(AttachmentViewDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentViewDesc::LoadStore::Clear: 
        case AttachmentViewDesc::LoadStore::DontCare: 
        case AttachmentViewDesc::LoadStore::Retain: 
        case AttachmentViewDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_ClearStencil: 
        case AttachmentViewDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;

        case AttachmentViewDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentViewDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentViewDesc::LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

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
            [name](const AttachmentViewDesc& desc) { return desc._viewName == name; });
        if (i != attachments.end())
            return (unsigned)std::distance(attachments.begin(), i);
        return ~0u;
    }

    static const AttachmentDesc* Find(IteratorRange<const AttachmentDesc*> attachmentResources, AttachmentName name)
    {
        for (const auto&a:attachmentResources)
            if (a._name == name)
                return &a;
        return nullptr;
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

    static bool IsRetained(AttachmentViewDesc::LoadStore loadStore)
    {
        return  loadStore == AttachmentViewDesc::LoadStore::Retain
            ||  loadStore == AttachmentViewDesc::LoadStore::Retain_RetainStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::Retain_ClearStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::DontCare_RetainStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::Clear_RetainStencil;
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
                [ai](const AttachmentViewDesc& adesc) { return adesc._viewName == ai; });
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

    static std::vector<VkSubpassDependency> CalculateDependencies(const FrameBufferDesc& layout, IteratorRange<const AttachmentDesc*> attachmentResources)
    {
        const auto& subpasses = layout.GetSubpasses();
        const auto& attachments = layout.GetAttachments();

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
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a));
            for (const auto& a:subpass._output)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), a));
            if (subpass._depthStencil != SubpassDesc::Unused)
                dependencies.push_back(FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), subpass._depthStencil));

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
                        [d](const AttachmentViewDesc& adesc) { return adesc._viewName == d->_attachment; });
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
        enum class AttachmentUsage : unsigned
        {
            Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
        };
    
        static unsigned GetAttachmentUsage(const FrameBufferDesc& layout, AttachmentName attachment)
        {
            unsigned result = 0u;
            for (auto& s:layout.GetSubpasses()) {
                auto i = std::find(s._input.begin(), s._input.end(), attachment);
                if (i != s._input.end()) 
                    result |= unsigned(Internal::AttachmentUsage::Input);
    
                auto o = std::find(s._output.begin(), s._output.end(), attachment);
                if (o != s._output.end()) 
                    result |= unsigned(Internal::AttachmentUsage::Output);
    
                if (s._depthStencil == attachment)
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
        auto attachments = layout.GetAttachments();
        auto subpasses = layout.GetSubpasses();

        std::vector<VkAttachmentDescription> attachmentDesc;
        attachmentDesc.reserve(attachments.size());
        for (auto&a:attachments) {
            const auto* resourceDesc = Find(attachmentResources, a._resourceName);
            assert(resourceDesc);

            auto formatFilter = a._window._format;
            if (formatFilter._aspect == TextureViewWindow::UndefinedAspect)
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

    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
        const FrameBufferProperties& props,
        VkRenderPass layout,
        const INamedResources& namedResources)
    : _layout(layout)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = fbDesc.GetAttachments();

        unsigned maxLayers = 0u;
        unsigned maxWidth = 0u;
        unsigned maxHeight = 0u;

        VkImageView rawViews[16];
        assert(attachments.size() < dimof(rawViews));
        for (unsigned c=0; c<(unsigned)attachments.size(); ++c) {
            const auto* rtv = namedResources.GetRTV(attachments[c]._viewName, attachments[c]._resourceName, attachments[c]._window);
            if (rtv && rtv->IsGood()) {
                rawViews[c] = rtv->GetImageView();
            } else {
                const auto* dsv = namedResources.GetDSV(attachments[c]._viewName, attachments[c]._resourceName, attachments[c]._window);
                assert(dsv);
                rawViews[c] = dsv->GetImageView();
            }

            // This is annoying -- we need to look for any resources with
            // array layers. Ideally all our resources should have the same
            // array layer count (if they don't, we just end up selecting the
            // largest value)
            auto* desc = namedResources.GetDesc(
                (attachments[c]._resourceName != ~0u) 
                ? attachments[c]._resourceName 
                : attachments[c]._viewName);
            if (desc) {
                maxLayers = std::max(maxLayers, desc->_arrayLayerCount);
                unsigned resWidth = 0u, resHeight = 0u;
                
                if (desc->_dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
                    resWidth = unsigned(desc->_width);
                    resHeight = unsigned(desc->_height);
                } else {
                    resWidth = unsigned(std::floor(props._outputWidth * desc->_width + 0.5f));
                    resHeight = unsigned(std::floor(props._outputHeight * desc->_height + 0.5f));
                }

                maxWidth = std::max(maxWidth, resWidth);
                maxHeight = std::max(maxHeight, resHeight);
            }
        }

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = layout;
        fb_info.attachmentCount = (uint32_t)attachments.size();
        fb_info.pAttachments = rawViews;
        fb_info.width = maxWidth;
        fb_info.height = maxHeight;
        fb_info.layers = std::max(1u, maxLayers);
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

    class FrameBufferCache::Pimpl 
    {
    public:
        std::vector<std::pair<uint64, VulkanUniquePtr<VkRenderPass>>> _layouts;
    };

    std::shared_ptr<FrameBuffer> FrameBufferCache::BuildFrameBuffer(
		const ObjectFactory& factory,
        const FrameBufferDesc& desc,
        const FrameBufferProperties& props,
        IteratorRange<const AttachmentDesc*> attachmentResources,
        const INamedResources& namedResources,
        uint64 hashName)
    {
        auto layout = BuildFrameBufferLayout(factory, desc, attachmentResources, props._samples);
        return std::make_shared<FrameBuffer>(factory, desc, props, layout, namedResources);
    }

    VkRenderPass FrameBufferCache::BuildFrameBufferLayout(
        const ObjectFactory& factory,
        const FrameBufferDesc& desc,
        IteratorRange<const AttachmentDesc*> attachmentResources,
        const TextureSamples& samples)
    {
        auto hash = desc.GetHash();
        auto i = LowerBound(_pimpl->_layouts, hash);
        if (i != _pimpl->_layouts.end() && i->first == hash)
            return i->second.get();

        auto rp = CreateRenderPass(factory, desc, attachmentResources, samples);
        auto result = rp.get();
        _pimpl->_layouts.insert(i, std::make_pair(hash, std::move(rp)));
        return result;
    }

    FrameBufferCache::FrameBufferCache()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferCache::~FrameBufferCache()
    {}

    INamedResources::~INamedResources() {}

}}

