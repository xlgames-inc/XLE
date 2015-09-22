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

namespace Utility
{
    template<typename Type> class DocElementHelper;
    template<typename Type> class InputStreamFormatter;
}

namespace RenderCore { namespace Assets { class VertexElement; }}
namespace RenderCore { namespace ColladaConversion
{
    class BindingConfig
    {
    public:
        using String = std::basic_string<utf8>;
        
        String AsNative(const utf8* inputStart, const utf8* inputEnd) const;
        bool IsSuppressed(const utf8* inputStart, const utf8* inputEnd) const;

        BindingConfig(const DocElementHelper<InputStreamFormatter<utf8>>& source);
        BindingConfig();
        ~BindingConfig();
    private:
        std::vector<std::pair<String, String>> _exportNameToBinding;
        std::vector<String> _bindingSuppressed;
    };

    class ImportConfiguration
    {
    public:
        const BindingConfig& GetResourceBindings() const { return _resourceBindings; }
        const BindingConfig& GetConstantBindings() const { return _constantsBindings; }
        const BindingConfig& GetVertexSemanticBindings() const { return _vertexSemanticBindings; }

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        ImportConfiguration(const ::Assets::ResChar filename[]);
        ImportConfiguration();
        ~ImportConfiguration();
    private:
        BindingConfig _resourceBindings;
        BindingConfig _constantsBindings;
        BindingConfig _vertexSemanticBindings;

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

    unsigned int    FloatBits(float input);
    unsigned int    FloatBits(double input);

    extern bool ImportCameras;

    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const Float3& localPosition, const Float4x4& localToWorld);
    void AddToBoundingBox(  std::pair<Float3, Float3>& boundingBox,
                            const void* vertexData, size_t vertexStride, size_t vertexCount,
                            const Assets::VertexElement& elementDesc, 
                            const Float4x4& localToWorld);
    std::pair<Float3, Float3>   InvalidBoundingBox();

    Assets::VertexElement FindPositionElement(const Assets::VertexElement elements[], size_t elementCount);
}}
