// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/BlockSerializer.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/Serialization.h"
#include "../../Utility/IteratorUtils.h"
#include <vector>
#include <functional>

namespace Utility { class OutputStream; }

namespace RenderCore { namespace Assets
{
    enum class TransformStackCommand : uint32
    {
        PushLocalToWorld,       // no parameters
        PopLocalToWorld,        // number of transforms to pop (ie, often 1, but sometimes we want to do multiple pops at once)

            //
            //      Transformation types.
            //      Just some basic transforms currently
            //          -- full 4x4 matrix
            //          -- translate
            //          -- rotation
            //          -- scale
            //
            //      These types are well suited the the Collada animation model.
            //
            //      But, in many cases, they should be converted into quaternion 
            //      representations during optimisation
            //
        TransformFloat4x4_Static,       // 4x4 transformation matrix (float)
        Translate_Static,               // X, Y, Z translation (float)
        RotateX_Static,                 // rotation around X (float)
        RotateY_Static,                 // rotation around X (float)
        RotateZ_Static,                 // rotation around X (float)
        Rotate_Static,                  // Axis X, Y, Z, rotation (float)
		RotateQuaternion_Static,		// Rotate through a quaternion
        UniformScale_Static,            // scalar (float)
        ArbitraryScale_Static,          // X, Y, Z scales (float)

        TransformFloat4x4_Parameter,    // 4x4 transformation matrix (float)
        Translate_Parameter,            // X, Y, Z translation (float)
        RotateX_Parameter,              // rotation around X (float)
        RotateY_Parameter,              // rotation around X (float)
        RotateZ_Parameter,              // rotation around X (float)
        Rotate_Parameter,               // Axis X, Y, Z, rotation (float)
		RotateQuaternion_Parameter,		// Rotate through a quaternion
        UniformScale_Parameter,         // scalar (float)
        ArbitraryScale_Parameter,       // X, Y, Z scales (float)

        WriteOutputMatrix,
        TransformFloat4x4AndWrite_Static,
        TransformFloat4x4AndWrite_Parameter,

        Comment
    };

	enum class AnimSamplerType { Float1, Float3, Float4, Float4x4, Quaternion };

            //////////////////////////////////////////////////////////

    class TransformationParameterSet
    {
    public:
        IteratorRange<const float*>     GetFloat1Parameters() const     { return MakeIteratorRange(_float1Parameters);      }
        IteratorRange<const Float3*>    GetFloat3Parameters() const     { return MakeIteratorRange(_float3Parameters);      }
        IteratorRange<const Float4*>    GetFloat4Parameters() const     { return MakeIteratorRange(_float4Parameters);      }
        IteratorRange<const Float4x4*>	GetFloat4x4Parameters() const	{ return MakeIteratorRange(_float4x4Parameters);	}

		IteratorRange<float*>			GetFloat1Parameters()			{ return MakeIteratorRange(_float1Parameters);      }
        IteratorRange<Float3*>			GetFloat3Parameters()			{ return MakeIteratorRange(_float3Parameters);      }
        IteratorRange<Float4*>			GetFloat4Parameters()			{ return MakeIteratorRange(_float4Parameters);      }
        IteratorRange<Float4x4*>		GetFloat4x4Parameters()			{ return MakeIteratorRange(_float4x4Parameters);	}

		uint32 AddParameter(float);
		uint32 AddParameter(Float3);
		uint32 AddParameter(Float4);
		uint32 AddParameter(const Float4x4&);
            
        TransformationParameterSet();
        TransformationParameterSet(TransformationParameterSet&& moveFrom);
        TransformationParameterSet& operator=(TransformationParameterSet&& moveFrom);
        TransformationParameterSet(const TransformationParameterSet& copyFrom);
        TransformationParameterSet& operator=(const TransformationParameterSet& copyFrom);

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer) const;

    private:
        SerializableVector<Float4x4>    _float4x4Parameters;
        SerializableVector<Float4>      _float4Parameters;
        SerializableVector<Float3>      _float3Parameters;
        SerializableVector<float>       _float1Parameters;
    };

        //////////////////////////////////////////////////////////

    void GenerateOutputTransformsFree(
        Float4x4                                    result[],
        size_t                                      resultCount,
        const TransformationParameterSet*           parameterSet,
        IteratorRange<const uint32*>                commandStream);

    void GenerateOutputTransformsFree(
        Float4x4                                    result[],
        size_t                                      resultCount,
        const TransformationParameterSet*           parameterSet,
        IteratorRange<const uint32*>                commandStream,
        const std::function<void(const Float4x4&, const Float4x4&)>&     debugIterator);

    void TraceTransformationMachine(
        std::ostream&                   outputStream,
        IteratorRange<const uint32*>    commandStream,
        std::function<std::string(unsigned)> outputMatrixToName,
        std::function<std::string(AnimSamplerType, unsigned)> parameterToName);

    class ITransformationMachineOptimizer
    {
    public:
        virtual bool CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const = 0;
        virtual void MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform) = 0;
        virtual ~ITransformationMachineOptimizer();
    };

    std::vector<uint32> OptimizeTransformationMachine(
        IteratorRange<const uint32*> input,
        ITransformationMachineOptimizer& optimizer);

}}

