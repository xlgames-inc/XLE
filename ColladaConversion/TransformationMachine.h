// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ColladaConversion.h"
#include "../RenderCore/Assets/NascentTransformationMachine.h"

namespace COLLADAFW
{
    template<class T> class PointerArray;
    class Transformation;
    typedef PointerArray<Transformation> TransformationPointerArray;
}

namespace RenderCore { namespace ColladaConversion
{
        ////////////////////////////////////////////////////////

    class NascentTransformationMachine_Collada : public RenderCore::Assets::NascentTransformationMachine
    {
    public:
        unsigned PushTransformations(const COLLADAFW::TransformationPointerArray& transformations, const char nodeName[]);

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const 
            { RenderCore::Assets::NascentTransformationMachine::Serialize(outputSerializer); }

        NascentTransformationMachine_Collada();
        NascentTransformationMachine_Collada(NascentTransformationMachine_Collada&& machine);
        NascentTransformationMachine_Collada& operator=(NascentTransformationMachine_Collada&& moveFrom);
        ~NascentTransformationMachine_Collada();
    };

}}