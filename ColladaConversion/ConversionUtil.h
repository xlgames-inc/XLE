// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/UTFUtils.h"
#include <vector>
#include <string>
#include <memory>

namespace Assets { class DependencyValidation; }

namespace RenderCore { namespace ColladaConversion
{
    class ImportConfiguration
    {
    public:
        using String = std::basic_string<utf8>;
        String AsNativeBinding(const utf8* inputStart, const utf8* inputEnd) const;
        bool IsSuppressed(const utf8* inputStart, const utf8* inputEnd) const;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        ImportConfiguration(const ::Assets::ResChar filename[]);
        ImportConfiguration();
        ~ImportConfiguration();
    private:
        std::vector<std::pair<String, String>> _exportNameToBinding;
        std::vector<String> _suppressed;
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };

    class NascentModelCommandStream;
    class TableOfObjects;

    std::pair<Float3, Float3> CalculateBoundingBox
        (
            const NascentModelCommandStream& scene,
            const TableOfObjects& objects,
            const Float4x4* transformsBegin, 
            const Float4x4* transformsEnd
        );
}}
