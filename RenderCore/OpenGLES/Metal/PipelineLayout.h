// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Utility/IteratorUtils.h"
#include <vector>

namespace RenderCore { namespace Metal_OpenGLES
{
    class PipelineLayoutConfig
    {
    public:
        /// If there is a fixed texture unit assigned for the given binding name, returns that index
        /// (otherwise returns ~0u)
        /// Generally only a few binding names should have fixed units -- most will require using
        /// one of the "flexible" texture units
        unsigned    GetFixedTextureUnit(uint64_t bindingName) const;

        /// Returns one of the texture units assigned to to "flexible" or remappable shader-to-shader.
        /// There are a limited number of these, depending on the hardware capabilites. If the index
        ///  is too high, then the return will be ~0u.
        unsigned    GetFlexibleTextureUnit(unsigned index) const;

        IteratorRange<const uint64_t*> GetFixedTextureBindingNames() const { return MakeIteratorRange(_fixedTextures); }

        PipelineLayoutConfig(IteratorRange<const uint64_t*> fixedTextureBindingNames);
        PipelineLayoutConfig();
        ~PipelineLayoutConfig();
    private:
        std::vector<unsigned> _unitsForFixedTextures;
        std::vector<unsigned> _unitsForFlexibleTextures;
        std::vector<uint64_t> _fixedTextures;
    };
}}
