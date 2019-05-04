// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <string>
#include <vector>

namespace RenderCore { namespace Assets { class ShaderPatchCollection; }}

namespace RenderCore { namespace Techniques
{
	/// <summary>Compiled and optimized version of RenderCore::Assets::ShaderPatchCollection</summary>
	/// A RenderCore::Assets::ShaderPatchCollection contains references to shader patches used by a material,
	/// however in that form it's not directly usable. We must expand the shader graphs and calculate the inputs
	/// and outputs before we can use it directly.
	/// 
	/// That's too expensive to do during the frame; so we do that during initialization phases and generate
	/// this object, the CompiledShaderPatchCollection
	class CompiledShaderPatchCollection
	{
	public:
		struct Patch 
		{
			uint64_t		_implementsHash;
			std::string		_scaffoldInFunction;		// scaffold function to use for patching in this particular implementation.
		};
		IteratorRange<const Patch*> GetPatches() const { return MakeIteratorRange(_patches); } 

		const std::string& GetSourceCode() const { return _srcCode; }

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		::Assets::DepValPtr _depVal;
		// std::vector<::Assets::DependentFileState> _dependencies;

		uint64_t GetGUID() const { return _guid; }

		// Settings for the illum delegate. We calculate and store this here. This will allow for
		// very efficient access by the illum delegate when we go to render this object.
		struct IllumDelegateAttachment
		{
			enum class IllumType { NoPerPixel, PerPixel, PerPixelAndEarlyRejection };
			IllumType _type;
		};
		IllumDelegateAttachment _illumDelegate;

		CompiledShaderPatchCollection(const RenderCore::Assets::ShaderPatchCollection& src);
		CompiledShaderPatchCollection();
		~CompiledShaderPatchCollection();
	private:
		uint64_t _guid = 0;
		std::string _srcCode;
		std::vector<Patch> _patches;
	};

}}
