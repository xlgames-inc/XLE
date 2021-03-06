// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/BlockSerializer.h"
#include "../../Math/Transformations.h"
#include "../../Math/Quaternion.h"
#include "../../Utility/Streams/SerializationUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <vector>
#include <functional>

namespace Utility { class OutputStream; }

namespace RenderCore { namespace Assets
{
    enum class TransformStackCommand : uint32_t
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
	const char* AsString(AnimSamplerType value);

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

		void Set(uint32_t index, float);
		void Set(uint32_t index, Float3);
		void Set(uint32_t index, Float4);
		void Set(uint32_t index, Quaternion);
		void Set(uint32_t index, const Float4x4&);
            
        TransformationParameterSet();
        TransformationParameterSet(TransformationParameterSet&& moveFrom);
        TransformationParameterSet& operator=(TransformationParameterSet&& moveFrom);
        TransformationParameterSet(const TransformationParameterSet& copyFrom);
        TransformationParameterSet& operator=(const TransformationParameterSet& copyFrom);

        void    SerializeMethod(::Assets::NascentBlockSerializer& outputSerializer) const;

    private:
        SerializableVector<Float4x4>    _float4x4Parameters;
        SerializableVector<Float4>      _float4Parameters;
        SerializableVector<Float3>      _float3Parameters;
        SerializableVector<float>       _float1Parameters;
    };

        //////////////////////////////////////////////////////////

    void GenerateOutputTransforms(
        IteratorRange<Float4x4*>					result,
        const TransformationParameterSet*           parameterSet,
        IteratorRange<const uint32_t*>                commandStream);

	/// <summary>For each output marker, calculate the immediate parent</summary>
	/// The parent of a given marker is defines as the first marker we encounter if we traverse back through
	/// the set of commands that affect the state of that given marker.
	///
	/// In effect, if the command stream is generated from a node hierarchy, then the parent will correspond
	/// to the parent from that source hierarchy (barring optimizations that have been performed post conversion) 
	/// This function writes out an array that is indexed by the child output marker index and contains the parent
	/// output marker index (or ~0u if there is none)
	///
	void CalculateParentPointers(
		IteratorRange<uint32_t*>					result,
		IteratorRange<const uint32_t*>				commandStream);

    void TraceTransformationMachine(
        std::ostream&                   outputStream,
        IteratorRange<const uint32_t*>    commandStream,
        std::function<std::string(unsigned)> outputMatrixToName,
        std::function<std::string(AnimSamplerType, unsigned)> parameterToName);

    class ITransformationMachineOptimizer
    {
    public:
        virtual bool CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const = 0;
        virtual void MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform) = 0;
        virtual ~ITransformationMachineOptimizer();
    };

	class TransformationMachineOptimizer_Null : public ITransformationMachineOptimizer
	{
	public:
		bool CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const { return false; } 
		void MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform) {};
	};

    std::vector<uint32_t> OptimizeTransformationMachine(
        IteratorRange<const uint32_t*> input,
        ITransformationMachineOptimizer& optimizer);

	std::vector<uint32_t> RemapOutputMatrices(
		IteratorRange<const uint32_t*> input,
		IteratorRange<const unsigned*> outputMatrixMapping);

}}

