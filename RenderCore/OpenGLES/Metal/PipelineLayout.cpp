// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineLayout.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    unsigned    PipelineLayoutConfig::GetFixedTextureUnit(uint64_t bindingName) const
    {
        auto i = std::lower_bound(_fixedTextures.begin(), _fixedTextures.end(), bindingName);
        if (i != _fixedTextures.end() && *i == bindingName) {
            auto index = std::distance(_fixedTextures.begin(), i);
            return _unitsForFixedTextures[index];
        }
        return ~0u;
    }

    unsigned    PipelineLayoutConfig::GetFlexibleTextureUnit(unsigned index) const
    {
        if (index < _unitsForFlexibleTextures.size())
            return _unitsForFlexibleTextures[index];
        return ~0u;
    }

    static unsigned GetMaxTextureUnitsAvailable()
    {
        GLint result = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &result);
        return result;
    }

    PipelineLayoutConfig::PipelineLayoutConfig(IteratorRange<const uint64_t*> fixedTextureBindingNames)
    {
        _fixedTextures = std::vector<uint64_t>(fixedTextureBindingNames.begin(), fixedTextureBindingNames.end());
        std::sort(_fixedTextures.begin(), _fixedTextures.end());    // fixed textures must be sorted, because we use lower_bound in GetFixedTextureUnit

        auto maxTextureUnits = GetMaxTextureUnitsAvailable();
        if (_fixedTextures.size() <= maxTextureUnits) {
            auto fixedTextureStartPoint = std::min(8u, maxTextureUnits - (unsigned)_fixedTextures.size());
            _unitsForFixedTextures.reserve(_fixedTextures.size());
            for (unsigned c=0; c<unsigned(_fixedTextures.size()); ++c)
                 _unitsForFixedTextures.push_back(fixedTextureStartPoint+c);
        } else {
            // if the number of fixed textures exceeds the available texture units, we have no choice but to have no fixed textures
            _fixedTextures.clear();
        }

        // remaining textures go into the "_unitsForFlexibleTextures"
        _unitsForFlexibleTextures.reserve(maxTextureUnits - _unitsForFixedTextures.size());
        for (unsigned c=0; c<maxTextureUnits; ++c) {
            if (std::find(_unitsForFixedTextures.begin(), _unitsForFixedTextures.end(), c) == _unitsForFixedTextures.end()) {
                _unitsForFlexibleTextures.push_back(c);
            }
        }
    }

    PipelineLayoutConfig::PipelineLayoutConfig()
    {
        auto maxTextureUnits = GetMaxTextureUnitsAvailable();
        for (unsigned c=0; c<maxTextureUnits; ++c)
            _unitsForFlexibleTextures.push_back(c);
    }
    PipelineLayoutConfig::~PipelineLayoutConfig() {}
}}
