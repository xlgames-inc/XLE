// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentObjectsSerialize.h"
#include "../Assets/AssetsCore.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../ConsoleRig/GlobalServices.h"

#if defined(PROJECT_COLLADA_CONVERSION)
    #define CONVERSION_API dll_export
#else
    #define CONVERSION_API dll_import
#endif

namespace RenderCore { namespace ColladaConversion
{
    class WorkingAnimationSet;

	class ICompileOperation
	{
	public:
		class TargetDesc
		{
		public:
			uint64 _type;
			const char* _name;
		};
		virtual unsigned			TargetCount() const = 0;
		virtual TargetDesc			GetTarget(unsigned idx) const = 0;
		virtual NascentChunkArray	SerializeTarget(unsigned idx) = 0;

		virtual ~ICompileOperation();
	};

	CONVERSION_API std::shared_ptr<ICompileOperation> CreateCompileOperation(const ::Assets::ResChar identifier[]);
	typedef std::shared_ptr<ICompileOperation> CreateCompileOperationFn(const ::Assets::ResChar identifier[]);

    CONVERSION_API std::shared_ptr<WorkingAnimationSet> CreateAnimationSet(const char name[]);
    CONVERSION_API void ExtractAnimations(WorkingAnimationSet& dest, const ICompileOperation& model, const char animName[]);
    CONVERSION_API NascentChunkArray SerializeAnimationSet(const WorkingAnimationSet& animset);

    typedef std::shared_ptr<WorkingAnimationSet> CreateAnimationSetFn(const char name[]);
    typedef void ExtractAnimationsFn(WorkingAnimationSet&, const ICompileOperation&, const char[]);
    typedef NascentChunkArray SerializeAnimationSetFn(const WorkingAnimationSet&);
}}

extern "C" CONVERSION_API ConsoleRig::LibVersionDesc GetVersionInformation();
extern "C" CONVERSION_API void AttachLibrary(ConsoleRig::GlobalServices&);
extern "C" CONVERSION_API void DetachLibrary();
