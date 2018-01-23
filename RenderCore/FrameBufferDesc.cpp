// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBufferDesc.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"

namespace RenderCore
{
    static bool HasAttachment(
        IteratorRange<const AttachmentViewDesc*> attachments,
        AttachmentName name)
    {
        for (const auto&i:attachments)
            if (i._viewName == name) return true;
        return false;
    }

	FrameBufferDesc::FrameBufferDesc(
		std::vector<SubpassDesc>&& subpasses,
		std::vector<AttachmentViewDesc>&& attachments)
	: _subpasses(std::move(subpasses))
	, _attachments(std::move(attachments))
	{
		// We can also have "implied" attachments. These are attachments that are referenced but not explicitly
        // declared. These only color, depth/stencil and resolve attachments can be implied. We must make some 
        // assumptions about format, layout, etc.
        for (auto&p: _subpasses) {
            for (auto& a:p._output)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentViewDesc{
                            a, a,
                            TextureViewWindow(),
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil});

            if (    p._depthStencil != SubpassDesc::Unused 
                &&  !HasAttachment(MakeIteratorRange(_attachments), p._depthStencil))
                _attachments.push_back(
                    AttachmentViewDesc{
                        p._depthStencil, p._depthStencil,
                        TextureViewWindow(),
                        AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                        AttachmentViewDesc::LoadStore::Retain_RetainStencil});

            for (auto& a:p._resolve)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentViewDesc{
                            a, a,
                            TextureViewWindow(),
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil});
        }

        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = DefaultSeed64;
        _hash = HashCombine(Hash64(AsPointer(_attachments.begin()), AsPointer(_attachments.end())), _hash);
        _hash = HashCombine(Hash64(AsPointer(_subpasses.begin()), AsPointer(_subpasses.end())), _hash);
    }

    FrameBufferDesc::FrameBufferDesc(
        IteratorRange<const SubpassDesc*> subpasses,
        IteratorRange<const AttachmentViewDesc*> attachments)
    : FrameBufferDesc(
		std::vector<SubpassDesc>(subpasses.begin(), subpasses.end()),
		std::vector<AttachmentViewDesc>(attachments.begin(), attachments.end()))
    {}

	FrameBufferDesc::FrameBufferDesc()
    : _hash(0)
    {
    }

    FrameBufferDesc::~FrameBufferDesc() {}

}

