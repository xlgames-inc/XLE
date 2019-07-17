// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NodeGraphSignature.h"
#include "../ConsoleRig/Log.h"

namespace GraphLanguage
{
	static void AddWithExistingCheck(
        std::vector<NodeGraphSignature::Parameter>& dst,
        const NodeGraphSignature::Parameter& param)
    {
	    // Look for another parameter with the same name...
	    auto existing = std::find_if(dst.begin(), dst.end(),
		    [&param](const NodeGraphSignature::Parameter& p) { return p._name == param._name && p._direction == param._direction; });
	    if (existing != dst.end()) {
		    // If we have 2 parameters with the same name, we're going to expect they
		    // also have the same type and semantic (otherwise we would need to adjust
		    // the name to avoid conflicts).
		    if (existing->_type != param._type || existing->_semantic != param._semantic) {
			    // Throw(::Exceptions::BasicLabel("Main function parameters with the same name, but different types/semantics (%s)", param._name.c_str()));
				Log(Debug) << "Main function parameters with the same name, but different types/semantics (" << param._name << ")" << std::endl;
			}
	    } else {
		    dst.push_back(param);
	    }
    }

	void NodeGraphSignature::AddParameter(const Parameter& param) { AddWithExistingCheck(_functionParameters, param); }
	void NodeGraphSignature::AddCapturedParameter(const Parameter& param) { AddWithExistingCheck(_capturedParameters, param); }
    void NodeGraphSignature::AddTemplateParameter(const TemplateParameter& param) { return _templateParameters.push_back(param); }

    NodeGraphSignature::NodeGraphSignature() {}
    NodeGraphSignature::~NodeGraphSignature() {}
}

