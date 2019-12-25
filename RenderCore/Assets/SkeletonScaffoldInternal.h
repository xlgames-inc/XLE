// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TransformationCommands.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"

namespace RenderCore { namespace Assets 
{
    class SkinningBindingBox;
    class TransformationParameterSet;
    class RawAnimationCurve;

    #pragma pack(push)
    #pragma pack(1)

    ////////////////////////////////////////////////////////////////////////////////////////////
    //      s k e l e t o n         //

    class SkeletonMachine
    {
    public:
        unsigned                            GetOutputMatrixCount() const        { return _outputMatrixCount; }
        const TransformationParameterSet&   GetDefaultParameters() const        { return _defaultParameters; }

        void GenerateOutputTransforms   (   IteratorRange<Float4x4*> output,
                                            const TransformationParameterSet*   parameterSet) const;

		void CalculateParentPointers(IteratorRange<unsigned*> output) const;

        class InputInterface
        {
        public:
            struct Parameter
            {
                uint64  _name = ~0ull;
                uint32  _index = 0u;
                AnimSamplerType  _type = (AnimSamplerType)0;
            };

            Parameter*  _parameters = nullptr;
            size_t      _parameterCount = 0;
        };

        class OutputInterface
        {
        public:
            uint64*     _outputMatrixNames = nullptr;
            size_t      _outputMatrixNameCount = 0;
        };

        const InputInterface&   GetInputInterface() const   { return _inputInterface; }
        const OutputInterface&  GetOutputInterface() const  { return _outputInterface; }

        SkeletonMachine();
        ~SkeletonMachine();
    protected:
        uint32*             _commandStream;
        size_t              _commandStreamSize;
        unsigned            _outputMatrixCount;

        InputInterface      _inputInterface;
        OutputInterface     _outputInterface;

		TransformationParameterSet      _defaultParameters;

        const uint32*   GetCommandStream()      { return _commandStream; }
        const size_t    GetCommandStreamSize()  { return _commandStreamSize; }
    };

    #pragma pack(pop)

}}

