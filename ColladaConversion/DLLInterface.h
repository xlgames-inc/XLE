// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/CompilerLibrary.h"
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

    CONVERSION_API std::shared_ptr<WorkingAnimationSet> CreateAnimationSet(const char name[]);
    CONVERSION_API void ExtractAnimations(WorkingAnimationSet& dest, const ::Assets::ICompileOperation& model, const char animName[]);
    CONVERSION_API ::Assets::NascentChunkArray SerializeAnimationSet(const WorkingAnimationSet& animset);

    typedef std::shared_ptr<WorkingAnimationSet> CreateAnimationSetFn(const char name[]);
    typedef void ExtractAnimationsFn(WorkingAnimationSet&, const ::Assets::ICompileOperation&, const char[]);
    typedef ::Assets::NascentChunkArray SerializeAnimationSetFn(const WorkingAnimationSet&);
}}

