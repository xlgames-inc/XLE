// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SimpleModelDeform.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Techniques
{
	auto DeformOperationFactory::CreateDeformOperations(
		StringSection<> initializer,
		const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold) -> InstantiationSet
	{
		auto* colon = XlFindChar(initializer, ':');
		auto* afterColon = initializer.end();
		if (colon) {
			afterColon = colon+1;
		} else {
			colon = initializer.end();
		}

		auto hash = Hash64(initializer.begin(), colon);
		auto i = LowerBound(_instantiationFunctions, hash);
		if (i==_instantiationFunctions.end() || i->first != hash)
			return {};

		return (i->second)(MakeStringSection(afterColon, initializer.end()), modelScaffold);
	}

	void DeformOperationFactory::RegisterDeformOperation(StringSection<> name, InitiationFunction&& fn)
	{
		auto hash = Hash64(name.begin(), name.end());
		auto i = LowerBound(_instantiationFunctions, hash);
		if (i!=_instantiationFunctions.end() && i->first == hash) {
			i->second = std::move(fn);
		} else {
			_instantiationFunctions.insert(i, std::make_pair(hash, std::move(fn)));
		}
	}

	DeformOperationFactory::DeformOperationFactory()
	{
	}

	DeformOperationFactory::~DeformOperationFactory()
	{
	}

	bool DeformOperationFactory::HasInstance() { return true; }
	DeformOperationFactory& DeformOperationFactory::GetInstance()
	{
		static DeformOperationFactory instance;
		return instance;
	}

	IDeformOperation::~IDeformOperation() {}
}}
