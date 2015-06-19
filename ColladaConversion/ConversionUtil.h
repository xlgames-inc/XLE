// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/InputLayout.h"
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
        bool IsBindingSuppressed(const utf8* inputStart, const utf8* inputEnd) const;
        String AsNativeVertexSemantic(const utf8* inputStart, const utf8* inputEnd) const;
        bool IsVertexSemanticSuppressed(const utf8* inputStart, const utf8* inputEnd) const;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

        ImportConfiguration(const ::Assets::ResChar filename[]);
        ImportConfiguration();
        ~ImportConfiguration();
    private:
        std::vector<std::pair<String, String>> _exportNameToBinding;
        std::vector<String> _bindingSuppressed;

        std::vector<std::pair<String, String>> _vertexSemanticRename;
        std::vector<String> _vertexSemanticSuppressed;

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
                            const Metal::InputElementDesc& elementDesc, 
                            const Float4x4& localToWorld);
    std::pair<Float3, Float3>   InvalidBoundingBox();

    Metal::InputElementDesc     FindPositionElement(const Metal::InputElementDesc elements[], size_t elementCount);
}}
