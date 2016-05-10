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
    static VkAttachmentLoadOp AsLoadOp(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::DontCare: 
        case AttachmentDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentDesc::LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case AttachmentDesc::LoadStore::Retain: 
        case AttachmentDesc::LoadStore::Retain_RetainStencil: 
        case AttachmentDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentDesc::LoadStore::Clear: 
        case AttachmentDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentDesc::LoadStore::Clear_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
    }

    static VkAttachmentStoreOp AsStoreOp(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::Clear: 
        case AttachmentDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentDesc::LoadStore::DontCare: 
        case AttachmentDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentDesc::LoadStore::DontCare_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case AttachmentDesc::LoadStore::Retain: 
        case AttachmentDesc::LoadStore::Retain_RetainStencil: 
        case AttachmentDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    static VkAttachmentLoadOp AsLoadOpStencil(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::Clear: 
        case AttachmentDesc::LoadStore::DontCare: 
        case AttachmentDesc::LoadStore::Retain: 
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        case AttachmentDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentDesc::LoadStore::DontCare_ClearStencil: 
        case AttachmentDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_LOAD_OP_CLEAR;

        case AttachmentDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentDesc::LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
    }

    static VkAttachmentStoreOp AsStoreOpStencil(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::Clear: 
        case AttachmentDesc::LoadStore::DontCare: 
        case AttachmentDesc::LoadStore::Retain: 
        case AttachmentDesc::LoadStore::Clear_ClearStencil: 
        case AttachmentDesc::LoadStore::DontCare_ClearStencil: 
        case AttachmentDesc::LoadStore::Retain_ClearStencil: 
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;

        case AttachmentDesc::LoadStore::Clear_RetainStencil: 
        case AttachmentDesc::LoadStore::DontCare_RetainStencil: 
        case AttachmentDesc::LoadStore::Retain_RetainStencil: 
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    static bool IsDepthStencilFormat(Format fmt)
    {
        auto comp = GetComponents(fmt);
        return comp == FormatComponents::Depth || comp == FormatComponents::DepthStencil || comp == FormatComponents::Stencil;
    }

    // static VkImageLayout AsShaderReadLayout(VkImageLayout layout)
    // {
    //     switch (layout)
    //     {
    //     case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    //         return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    //     case VK_IMAGE_LAYOUT_GENERAL:
    //         return VK_IMAGE_LAYOUT_GENERAL;
    //     default:
    //         return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //     }
    // }

    static VkImageLayout AsShaderReadLayout(Format format)
    {
        if (IsDepthStencilFormat(format))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    static unsigned FindAttachmentIndex(IteratorRange<const AttachmentDesc*> attachments, AttachmentDesc::Name name)
    {
        auto i = std::find_if(
            attachments.begin(), attachments.end(), 
            [name](const AttachmentDesc& desc) { return desc._name == name; });
        if (i != attachments.end())
            return (unsigned)std::distance(attachments.begin(), i);
        return ~0u;
    }

    struct SubpassDep
    {
        unsigned                _srcSubpassIndex;
        AttachmentDesc::Name    _attachment;
        VkAccessFlags           _srcAccessFlags;
        VkAccessFlags           _dstAccessFlags;

        static bool Compare(const SubpassDep& lhs, const SubpassDep& rhs)
        {
            if (lhs._srcSubpassIndex < rhs._srcSubpassIndex) return true;
            if (lhs._srcSubpassIndex > rhs._srcSubpassIndex) return false;
            if (lhs._attachment < rhs._attachment) return true;
            if (lhs._attachment > rhs._attachment) return false;
            return lhs._srcAccessFlags < rhs._srcAccessFlags;
        }
    };

    static SubpassDep FindWriter(IteratorRange<const SubpassDesc*> subpasses, AttachmentDesc::Name attachment)
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

    static bool IsRetained(AttachmentDesc::LoadStore loadStore)
    {
        return  loadStore == AttachmentDesc::LoadStore::Retain
            ||  loadStore == AttachmentDesc::LoadStore::Retain_RetainStencil
            ||  loadStore == AttachmentDesc::LoadStore::Retain_ClearStencil
            ||  loadStore == AttachmentDesc::LoadStore::DontCare_RetainStencil
            ||  loadStore == AttachmentDesc::LoadStore::Clear_RetainStencil;
    }

    static void BuildOutputDeps(
        std::vector<SubpassDep>& result,
        IteratorRange<const AttachmentDesc::Name*> attachmentsToTest,
        IteratorRange<const AttachmentDesc*> attachments,
        VkAccessFlags srcAccessFlags)
    {
        for (auto ai:attachmentsToTest) {
            auto a = std::find_if(
                attachments.begin(), attachments.end(),
                [ai](const AttachmentDesc& adesc) { return adesc._name == ai; });
            if (a == attachments.end()) { assert(0); continue; }   // couldn't find it?

            bool isRetainStore = IsRetained(a->_storeToNextPhase);
            if (!isRetainStore) continue;

            VkAccessFlags dstAccessFlags = 0;
            if (a->_flags & AttachmentDesc::Flags::ShaderResource)
                dstAccessFlags |= VK_ACCESS_SHADER_READ_BIT;
            if (a->_flags & AttachmentDesc::Flags::TransferSource)
                dstAccessFlags |= VK_ACCESS_TRANSFER_READ_BIT;
            if (!dstAccessFlags) continue;

            result.push_back(SubpassDep{0, ai, srcAccessFlags, dstAccessFlags});
        }
    }

    static std::vector<VkSubpassDependency> CalculateDependencies(const FrameBufferDesc& layout)
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
            std::vector<SubpassDep> dependencies;
            const auto& subpass = subpasses[c];
            for (unsigned i=0;i<(unsigned)subpass._input.size(); ++i)
                dependencies.push_back(
                    FindWriter(MakeIteratorRange(subpasses.begin(), subpasses.begin()+c), subpass._input[i]));

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
                        [d](const AttachmentDesc& adesc) { return adesc._name == d->_attachment; });
                    if (a != attachments.end()) {
                        srcAccessFlags = IsDepthStencilFormat(a->_format)
                            ?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._output), attachments, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._preserve), attachments, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            BuildOutputDeps(outputDeps, MakeIteratorRange(finalSubpass._resolve), attachments, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            if (finalSubpass._depthStencil != SubpassDesc::Unused)
                BuildOutputDeps(
                    outputDeps, 
                    MakeIteratorRange(&finalSubpass._depthStencil, &finalSubpass._depthStencil + 1),
                    attachments, 
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

        return result;
    }

    VulkanUniquePtr<VkRenderPass> CreateRenderPass(
        const Metal_Vulkan::ObjectFactory& factory,
        const FrameBufferDesc& layout)
    {
        auto attachments = layout.GetAttachments();
        auto subpasses = layout.GetSubpasses();

        std::vector<VkAttachmentDescription> attachmentDesc;
        attachmentDesc.reserve(attachments.size());
        for (auto&a:attachments) {
            VkAttachmentDescription desc;
            desc.flags = 0;
            desc.format = AsVkFormat(a._format);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = AsLoadOp(a._loadFromPreviousPhase);
            desc.storeOp = AsStoreOp(a._storeToNextPhase);
            desc.stencilLoadOp = AsLoadOpStencil(a._loadFromPreviousPhase);
            desc.stencilStoreOp = AsStoreOpStencil(a._storeToNextPhase);

            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool isDepthStencil = IsDepthStencilFormat(a._format);
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
            if (a._flags & AttachmentDesc::Flags::ShaderResource) {
                desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            // desc.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (a._name == 0u) {
                // we assume that name "0" is always bound to a presentable buffer
                assert(!isDepthStencil);
                desc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            } 
            
            if (a._flags & AttachmentDesc::Flags::Multisampled)
                desc.samples = AsSampleCountFlagBits(layout.GetSamples());

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
                    auto attachmentIndex = FindAttachmentIndex(attachments, a);
                    assert(attachmentIndex < attachmentDesc.size());
                    // auto layout = AsShaderReadLayout(attachmentDesc[attachmentIndex].finalLayout);
                    auto layout = AsShaderReadLayout(attachments[attachmentIndex]._format);
                    attachReferences.push_back(VkAttachmentReference{attachmentIndex, layout});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pInputAttachments = (const VkAttachmentReference*)(beforeInputs+1);
            desc.inputAttachmentCount = uint32_t(attachReferences.size() - beforeInputs);

            auto beforeOutputs = attachReferences.size();
            for (auto& a:p._output) {
                if (a != SubpassDesc::Unused) {
                    auto attachmentIndex = FindAttachmentIndex(attachments, a);
                    assert(attachmentIndex < attachmentDesc.size());
                    auto layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;        // basically should be VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL, I guess?
                    attachReferences.push_back(VkAttachmentReference{attachmentIndex, layout});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pColorAttachments = (const VkAttachmentReference*)(beforeOutputs+1);
            desc.colorAttachmentCount = uint32_t(attachReferences.size() - beforeOutputs);
            desc.pResolveAttachments = nullptr; // not supported

            if (p._depthStencil != SubpassDesc::Unused) {
                auto attachmentIndex = FindAttachmentIndex(attachments, p._depthStencil);
                desc.pDepthStencilAttachment = (const VkAttachmentReference*)(attachReferences.size()+1);
                attachReferences.push_back(VkAttachmentReference{attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
            } else {
                desc.pDepthStencilAttachment = nullptr;
            }

            auto beforePreserve = preserveAttachments.size();
            for (auto&a:p._preserve) {
                auto attachmentIndex = FindAttachmentIndex(attachments, a);
                assert(attachmentIndex < attachmentDesc.size());
                preserveAttachments.push_back(attachmentIndex);
            }
            desc.pPreserveAttachments = (const uint32_t*)(beforePreserve+1);
            desc.preserveAttachmentCount = uint32_t(preserveAttachments.size() - beforePreserve);

            if (!p._resolve.empty()) {
                assert(p._resolve.size() == p._output.size());
                auto beforeResolve = attachReferences.size();
                for (auto&a:p._resolve) {
                    auto attachmentIndex = FindAttachmentIndex(attachments, a);
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

        auto dependencies = CalculateDependencies(layout);

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

    namespace Internal
    {
        enum class AttachmentUsage : unsigned
        {
            Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
        };
    }
    static unsigned GetAttachmentUsage(const FrameBufferDesc& layout, AttachmentDesc::Name attachment)
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

    static Format AsSRVFormat(Format input)
    {
        return input;
    }

    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
		VkRenderPass layout,
		const FrameBufferProperties& props,
        NamedResources& namedResources)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = fbDesc.GetAttachments();
        _views.reserve(attachments.size());
        std::vector<VkImageView> rawViews;
        rawViews.reserve(attachments.size());
        for (const auto&a:attachments) {

            // First, look for an existing resource that is bound with this name.
            const TextureView* existingView = namedResources.GetRTV(a._name);
            if (existingView == nullptr)
                existingView = namedResources.GetSRV(a._name);
            if (existingView != nullptr) {
                rawViews.push_back(existingView->GetImageView());
                _views.push_back(*existingView);
                continue;
            }

            // We need to calculate the dimensions, format, samples and bind flags for this
            // attachment. All of the information we need should be defined as part of the frame
            // buffer layout description.

            // note -- how do the frame buffer dimensions relate to the actual image dimensions?
            //          the documentation suggest that the frame buffer dims should always be equal
            //          or smaller to the image views...?
            unsigned attachmentWidth, attachmentHeight;
            if (a._dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
                attachmentWidth = unsigned(a._width);
                attachmentHeight = unsigned(a._height);
            } else {
                attachmentWidth = unsigned(std::floor(props._outputWidth * a._width + 0.5f));
                attachmentHeight = unsigned(std::floor(props._outputHeight * a._height + 0.5f));
            }

            auto desc = CreateDesc(
                0, 0, 0, 
                TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(props._outputLayers)),
                "attachment");

            if (a._flags & AttachmentDesc::Flags::Multisampled)
                desc._textureDesc._samples = fbDesc.GetSamples();

            // Look at how the attachment is used by the subpasses to figure out what the
            // bind flags should be.

            // todo --  Do we also need to consider what happens to the image after 
            //          the render pass has finished? Resources that are in "output", 
            //          "depthStencil", or "preserve" in the final subpass could be used
            //          in some other way afterwards. For example, one render pass could
            //          generate shadow textures for uses in future render passes?
            auto usage = GetAttachmentUsage(fbDesc, a._name);
            if (usage & unsigned(Internal::AttachmentUsage::Input)
                || a._flags & AttachmentDesc::Flags::ShaderResource) {
                desc._bindFlags |= BindFlag::ShaderResource;
                desc._gpuAccess |= GPUAccess::Read;
            }

            if (usage & unsigned(Internal::AttachmentUsage::Output)) {
                desc._bindFlags |= BindFlag::RenderTarget;
                desc._gpuAccess |= GPUAccess::Write;
            }

            if (usage & unsigned(Internal::AttachmentUsage::DepthStencil)) {
                desc._bindFlags |= BindFlag::DepthStencil;
                desc._gpuAccess |= GPUAccess::Write;
            }

            if (a._flags & AttachmentDesc::Flags::TransferSource) {
                desc._bindFlags |= BindFlag::TransferSrc;
                desc._gpuAccess |= GPUAccess::Read;
            }

            // note -- it might be handy to have a cache of "device memory" that could be reused here?
            auto image = Resource::Allocate(factory, desc);
            TextureViewWindow srvWindow;
            srvWindow._format = AsSRVFormat(desc._textureDesc._format);
            ShaderResourceView view(factory, image, srvWindow);

            // register in the named resources (if it's marked as a store resource, or used as input somewhere)
            if (IsRetained(a._storeToNextPhase) || (usage & unsigned(Internal::AttachmentUsage::Input)))
                namedResources.Bind(a._name, view);

            rawViews.push_back(view.GetImageView());
            _views.emplace_back(std::move(view));
        }

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = layout;
        fb_info.attachmentCount = (uint32_t)rawViews.size();
        fb_info.pAttachments = AsPointer(rawViews.begin());
        fb_info.width = props._outputWidth;
        fb_info.height = props._outputHeight;
        fb_info.layers = std::max(1u, props._outputLayers);
        _underlying = factory.CreateFramebuffer(fb_info);

        // todo --  do we need to create a "patch up" command buffer to assign the starting image layouts
        //          for all of the images we created?
    }

    const TextureView& FrameBuffer::GetAttachment(unsigned index) const
    {
        if (index >= _views.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetAttachment()"));
        return _views[index];
    }

	FrameBuffer::FrameBuffer() {}

    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void            RenderPassInstance::NextSubpass()
    {
        if (_attachedContext)
            _attachedContext->CmdNextSubpass(VK_SUBPASS_CONTENTS_INLINE);
    }

    void            RenderPassInstance::End()
    {
        if (_attachedContext)
            _attachedContext->EndRenderPass();
    }

    const TextureView&  RenderPassInstance::GetAttachment(unsigned index)
    {
        // We can call this function during the render pass... However normally if we 
        // want to use a render target, we should do it after the render pass has been
        // ended (with RenderPassInstance::End())
        return _frameBuffer->GetAttachment(index);
    }

    RenderPassInstance::RenderPassInstance(
        DeviceContext& context,
        const FrameBufferDesc& layout,
		const FrameBufferProperties& props,
        uint64 hashName,
        FrameBufferCache& cache,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass
        auto renderPass = cache.BuildFrameBufferLayout(context.GetFactory(), layout);
        _frameBuffer = cache.BuildFrameBuffer(
            context.GetFactory(), layout, 
            renderPass,
            props, context.GetNamedResources(), hashName);
        assert(_frameBuffer);
        auto ext = VectorPattern<unsigned, 2>{0,0}; // beginInfo._extent;
        auto offset = VectorPattern<int, 2>{0,0}; // beginInfo._offset;
        if (ext[0] == 0 && ext[1] == 0) {
            ext[0] = props._outputWidth - offset[0];
            ext[1] = props._outputHeight - offset[1];
        }
        context.BeginRenderPass(
            renderPass, *_frameBuffer, layout.GetSamples(), offset, ext, 
            beginInfo._clearValues);
        _attachedContext = &context;
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
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
		VkRenderPass layout,
		const FrameBufferProperties& props,
        NamedResources& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, layout, props, namedResources);
    }

    VkRenderPass FrameBufferCache::BuildFrameBufferLayout(
        const ObjectFactory& factory,
        const FrameBufferDesc& desc)
    {
        auto hash = desc.GetHash();
        auto i = LowerBound(_pimpl->_layouts, hash);
        if (i != _pimpl->_layouts.end() && i->first == hash)
            return i->second.get();

        auto rp = CreateRenderPass(factory, desc);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const unsigned s_maxBoundTargets = 64;

    class NamedResources::Pimpl
    {
    public:
        ShaderResourceView  _srv[s_maxBoundTargets];
        TextureView         _rtv[s_maxBoundTargets];
    };
    
    const ShaderResourceView*   NamedResources::GetSRV(AttachmentDesc::Name name) const
    {
        if (name >= s_maxBoundTargets) return nullptr;
        if (!_pimpl->_srv[name].IsGood()) return nullptr;
        return &_pimpl->_srv[name];
    }

    const TextureView*     NamedResources::GetRTV(AttachmentDesc::Name name) const
    {
        if (name >= s_maxBoundTargets) return nullptr;
        if (!_pimpl->_rtv[name].IsGood()) return nullptr;
        return &_pimpl->_rtv[name];
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const ShaderResourceView& srv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_srv[name] = srv;
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const RenderTargetView& rtv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_rtv[name] = rtv;
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const DepthStencilView& dsv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_rtv[name] = dsv;
    }

    void NamedResources::UnbindAll()
    {
        for (unsigned c=0; c<s_maxBoundTargets; ++c) {
            _pimpl->_srv[c] = ShaderResourceView();
            _pimpl->_rtv[c] = TextureView();
        }
    }

    NamedResources::NamedResources()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    NamedResources::~NamedResources()
    {}

}}

