// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateResources.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Assets 
{

    class ColladaCompiler : public ::Assets::IntermediateResources::IResourceCompiler
    {
    public:
        std::shared_ptr<::Assets::PendingCompileMarker> PrepareResource(
            uint64 typeCode, 
            const ::Assets::ResChar* initializers[], unsigned initializerCount,
            const ::Assets::IntermediateResources::Store& destinationStore);

        void StallOnPendingOperations(bool cancelAll);

        static const uint64 Type_Model = ConstHash64<'Mode', 'l'>::Value;
        static const uint64 Type_AnimationSet = ConstHash64<'Anim', 'Set'>::Value;
        static const uint64 Type_Skeleton = ConstHash64<'Skel', 'eton'>::Value;

        ColladaCompiler();
        ~ColladaCompiler();
    protected:
        class Pimpl;
        std::shared_ptr<Pimpl> _pimpl;
    };

}}

