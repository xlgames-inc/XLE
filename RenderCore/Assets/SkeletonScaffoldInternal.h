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

        void GenerateOutputTransforms   (   Float4x4 output[], unsigned outputCount,
                                            const TransformationParameterSet*   parameterSet) const;

        using DebugIterator = std::function<void(const Float4x4&, const Float4x4&)>;
        void GenerateOutputTransforms   (   Float4x4 output[], unsigned outputCount,
                                            const TransformationParameterSet*   parameterSet,
                                            const DebugIterator& debugIterator) const;

        class InputInterface
        {
        public:
            struct Parameter
            {
                uint64  _name;
                uint32  _index;
                AnimSamplerType  _type;
            };

            Parameter*  _parameters;
            size_t      _parameterCount;
        };

        class OutputInterface
        {
        public:
            uint64*     _outputMatrixNames;
            size_t      _outputMatrixNameCount;
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

