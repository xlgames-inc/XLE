// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DescriptorSetInstantiation.h"
#include "NodeGraphSignature.h"
#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../RenderCore/UniformsStream.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include <set>
#include <unordered_map>
#include <iostream>

namespace ShaderSourceParser
{
	static std::string MakeGlobalName(const std::string& str)
	{
		auto i = str.find('.');
		if (i != std::string::npos)
			return str.substr(i+1);
		return str;
	}

	std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout> MakeMaterialDescriptorSet(
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures,
		RenderCore::ShaderLanguage shaderLanguage,
		std::ostream& warningStream)
	{
		using NameAndType = RenderCore::Assets::PredefinedCBLayout::NameAndType;
		struct WorkingCB
		{
			std::vector<NameAndType> _cbElements;
			ParameterBox _defaults;
		};
		std::unordered_map<std::string, WorkingCB> workingCBs;
		std::set<std::string> objectsAlreadyStored;
		auto result = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>();

		// hack -- skip DiffuseTexture and NormalsTexture, because these are provided by the system headers
		// objectsAlreadyStored.insert("DiffuseTexture");
		// objectsAlreadyStored.insert("NormalsTexture");

		using DescriptorSlot = RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot;

		for (const auto&c : captures) {
			if (c._direction != GraphLanguage::ParameterDirection::In)
				continue;

			DescriptorSlot newSlot;			
			newSlot._type = RenderCore::ShaderLangTypeNameAsDescriptorType(c._type);

			// If we didn't get a descriptor slot type from the type name, we'll treat this as a
			// constant within a constant buffer
			if (newSlot._type == RenderCore::DescriptorType::Unknown) {
				auto fmt = RenderCore::ShaderLangTypeNameAsTypeDesc(c._type);
				if (fmt._type == ImpliedTyping::TypeCat::Void) {
					warningStream << "\t// Could not convert type (" << c._type << ") to shader language type for capture (" << c._name << "). Skipping cbuffer entry." << std::endl;
					continue;
				}

				std::string cbName, memberName;
				auto i = c._name.find('.');
				if (i != std::string::npos) {
					cbName = c._name.substr(0, i);
					memberName = c._name.substr(i+1);
				} else {
					cbName = "BasicMaterialConstants";
					memberName = c._name;
				}

				auto cbi = workingCBs.find(cbName);
				if (cbi == workingCBs.end())
					cbi = workingCBs.insert(std::make_pair(cbName, WorkingCB{})).first;

				cbi->second._cbElements.push_back(NameAndType{ memberName, fmt });
				if (!c._default.empty())
					cbi->second._defaults.SetParameter(
						MakeStringSection(memberName).Cast<utf8>(),
						MakeStringSection(c._default));

				newSlot._cbIdx = (unsigned)std::distance(workingCBs.begin(), cbi);
				newSlot._name = cbName;
				newSlot._type = RenderCore::DescriptorType::ConstantBuffer;
			} else {
				newSlot._name = MakeGlobalName(c._name);
			}

			if (objectsAlreadyStored.find(newSlot._name) == objectsAlreadyStored.end()) {
				objectsAlreadyStored.insert(newSlot._name);
				result->_slots.push_back(newSlot);
			}
		}

		for (auto&cb:workingCBs) {
			if (cb.second._cbElements.empty())
				continue;

			// Sort first in alphabetical order, and then optimize for
			// type packing. This ensures that we get the same output layout for a given
			// input, regardless of the input's original ordering.
			std::sort(
				cb.second._cbElements.begin(), cb.second._cbElements.end(),
				[](const NameAndType& lhs, const NameAndType& rhs) {
					return lhs._name < rhs._name;
				});
			RenderCore::Assets::PredefinedCBLayout::OptimizeElementOrder(MakeIteratorRange(cb.second._cbElements), shaderLanguage);

			
			auto layout = std::make_shared<RenderCore::Assets::PredefinedCBLayout>(
				MakeIteratorRange(cb.second._cbElements), cb.second._defaults);
			result->_constantBuffers.emplace_back(std::move(layout));
		}

		return result;
	}
}
