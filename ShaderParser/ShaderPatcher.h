// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphSignature.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/StringUtils.h"
#include "../Core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <iosfwd>

namespace GraphLanguage { class NodeGraph; class NodeGraphSignature; }
namespace RenderCore { namespace Techniques { class PredefinedCBLayout; } }

namespace ShaderSourceParser
{
	std::string GenerateCapturesCBuffer(
		StringSection<> name,
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures);

	std::shared_ptr<RenderCore::Techniques::PredefinedCBLayout> MakePredefinedCBLayout(
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures,
		std::ostream& warningStream);

    struct PreviewOptions
    {
    public:
        enum class Type { Object, Chart };
        Type _type;
        std::string _outputToVisualize;
		using VariableRestrictions = std::vector<std::pair<std::string, std::string>>;
        VariableRestrictions _variableRestrictions;
    };

    std::string GenerateStructureForPreview(
        StringSection<> graphName, 
        const GraphLanguage::NodeGraphSignature& interf, 
        const PreviewOptions& previewOptions = { PreviewOptions::Type::Object, std::string(), PreviewOptions::VariableRestrictions() });

	std::string GenerateStructureForTechniqueConfig(
		const GraphLanguage::NodeGraphSignature& interf, 
		StringSection<char> graphName);

	std::string GenerateScaffoldFunction(
		const GraphLanguage::NodeGraphSignature& outputSignature, 
		const GraphLanguage::NodeGraphSignature& generatedFunctionSignature, 
		StringSection<char> scaffoldFunctionName,
		StringSection<char> implementationFunctionName);
}

