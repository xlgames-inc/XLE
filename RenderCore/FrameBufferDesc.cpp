// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FrameBufferDesc.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"

namespace RenderCore
{
    SubpassDesc::SubpassDesc()
    : _depthStencil(Unused)
    {
    }

    SubpassDesc::SubpassDesc(
        IteratorRange<const AttachmentDesc::Name*> output,
        unsigned depthStencil,
        IteratorRange<const AttachmentDesc::Name*> input, 
        IteratorRange<const AttachmentDesc::Name*> preserve,
        IteratorRange<const AttachmentDesc::Name*> resolve)
    : _input(input.begin(), input.end())
    , _output(output.begin(), output.end())
    , _depthStencil(depthStencil)
    , _preserve(preserve.begin(), preserve.end())
    , _resolve(resolve.begin(), resolve.end())
    {
    }

    static bool HasAttachment(
        IteratorRange<const AttachmentDesc*> attachments,
        AttachmentDesc::Name name)
    {
        for (const auto&i:attachments)
            if (i._name == name) return true;
        return false;
    }

    FrameBufferDesc::FrameBufferDesc(
        IteratorRange<const AttachmentDesc*> attachments,
        IteratorRange<const SubpassDesc*> subpasses,
        const TextureSamples& samples)
    : _attachments(attachments.begin(), attachments.end())
    , _subpasses(subpasses.begin(), subpasses.end())
    , _samples(samples)
    {
        // We can also have "implied" attachments. These are attachments that are referenced but not explicitly
        // declared. These only color, depth/stencil and resolve attachments can be implied. We must make some 
        // assumptions about format, layout, etc.
        for (auto&p:subpasses) {
            for (auto& a:p._output)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentDesc{
                            AttachmentDesc::DimensionsMode::OutputRelative,
                            1.f, 1.f,
                            Format::R8G8B8A8_UNORM_SRGB,
                            AttachmentDesc::LoadStore::Retain_RetainStencil,
                            AttachmentDesc::LoadStore::Retain_RetainStencil,
                            a, 0});

            if (    p._depthStencil != SubpassDesc::Unused 
                &&  !HasAttachment(MakeIteratorRange(_attachments), p._depthStencil))
                _attachments.push_back(
                    AttachmentDesc{
                        AttachmentDesc::DimensionsMode::OutputRelative,
                        1.f, 1.f,
                        Format::D24_UNORM_S8_UINT,
                        AttachmentDesc::LoadStore::Retain_RetainStencil,
                        AttachmentDesc::LoadStore::Retain_RetainStencil,
                        p._depthStencil, 0});

            for (auto& a:p._resolve)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentDesc{
                            AttachmentDesc::DimensionsMode::OutputRelative,
                            1.f, 1.f,
                            Format::R8G8B8A8_UNORM_SRGB,
                            AttachmentDesc::LoadStore::Retain_RetainStencil,
                            AttachmentDesc::LoadStore::Retain_RetainStencil,
                            a, 0});
        }

        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = DefaultSeed64;
        _hash = HashCombine(samples._sampleCount | samples._samplingQuality << 8, _hash);
        _hash = HashCombine(Hash64(AsPointer(_attachments.begin()), AsPointer(_attachments.end())), _hash);
        _hash = HashCombine(Hash64(AsPointer(_subpasses.begin()), AsPointer(_subpasses.end())), _hash);
    }

	FrameBufferDesc::FrameBufferDesc()
    : _samples(TextureSamples::Create())
    , _hash(0)
    {
    }

    FrameBufferDesc::~FrameBufferDesc() {}
}

