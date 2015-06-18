// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OCMisc.h"
#include "../NascentMaterial.h"
#include "../../Assets/Assets.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWUniqueId.h>
    #include <COLLADAFWImage.h>
#pragma warning(pop)

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    ReferencedTexture       Convert(const COLLADAFW::Image* image)
    {
        if (image->getSourceType() != COLLADAFW::Image::SOURCE_TYPE_URI) {
            ThrowException(FormatError("Cannot load image (%s). Only URI type textures are supported.", image->getName().c_str()));
        }

        const COLLADABU::URI& originalURI = image->getImageURI();
        std::string nativePath = originalURI.toNativePath();
        if (nativePath.size() >= 2 && (nativePath[0] == '\\' || nativePath[0] == '/') && (nativePath[1] == '\\' || nativePath[1] == '/')) {
                // skip over the first 2 characters if we begin with \\ or //
            nativePath = nativePath.substr(2);  
        }
        return ReferencedTexture(nativePath);
    }

    ObjectGuid Convert(const COLLADAFW::UniqueId& input)
    {
        return ObjectGuid(input.getObjectId(), input.getFileId());
    }

    RenderCore::Assets::AnimationParameterId BuildAnimParameterId(const COLLADAFW::UniqueId& input)
    {
        return (RenderCore::Assets::AnimationParameterId)input.getObjectId();
    }

}}
