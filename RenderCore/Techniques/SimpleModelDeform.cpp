// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SimpleModelDeform.h"
#include "Services.h"
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

		return (i->second._instFunction)(MakeStringSection(afterColon, initializer.end()), modelScaffold);
	}

	auto DeformOperationFactory::RegisterDeformOperation(StringSection<> name, InitiationFunction&& fn) -> RegisteredDeformId
	{
		RegisteredDeformId result = _nextDeformId++;
		auto hash = Hash64(name.begin(), name.end());
		auto i = LowerBound(_instantiationFunctions, hash);
		if (i!=_instantiationFunctions.end() && i->first == hash) {
			i->second = {std::move(fn), result};
		} else {
			_instantiationFunctions.insert(i, std::make_pair(hash, RegisteredDeformOp{std::move(fn), result}));
		}
		return result;
	}

	void DeformOperationFactory::DeregisterDeformOperation(RegisteredDeformId deformId)
	{
		auto i = std::remove_if(
			_instantiationFunctions.begin(),
			_instantiationFunctions.end(),
			[deformId](const std::pair<uint64_t, RegisteredDeformOp>& i) {
				return i.second._deformId == deformId;
			});
		_instantiationFunctions.erase(i, _instantiationFunctions.end());
	}

	DeformOperationFactory::DeformOperationFactory()
	{
		_nextDeformId = 1;
	}

	DeformOperationFactory::~DeformOperationFactory()
	{
	}

	DeformOperationFactory& DeformOperationFactory::GetInstance()
	{
		return Services::GetDeformOperationFactory();
	}

	IDeformOperation::~IDeformOperation() {}
}}
