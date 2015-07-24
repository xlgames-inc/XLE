// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/BlockSerializer.h"
#include "../../ConsoleRig/Log.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/Streams/Serialization.h"

namespace Utility { class OutputStream; }

namespace RenderCore { namespace Assets
{
    namespace TransformStackCommand
    {
        enum Enum
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
            UniformScale_Static,            // scalar (float)
            ArbitraryScale_Static,          // X, Y, Z scales (float)

            TransformFloat4x4_Parameter,    // 4x4 transformation matrix (float)
            Translate_Parameter,            // X, Y, Z translation (float)
            RotateX_Parameter,              // rotation around X (float)
            RotateY_Parameter,              // rotation around X (float)
            RotateZ_Parameter,              // rotation around X (float)
            Rotate_Parameter,               // Axis X, Y, Z, rotation (float)
            UniformScale_Parameter,         // scalar (float)
            ArbitraryScale_Parameter,       // X, Y, Z scales (float)

            WriteOutputMatrix
        };
    }

            //////////////////////////////////////////////////////////

    class TransformationParameterSet : noncopyable
    {
    public:
        struct Type { enum Enum { Float1, Float3, Float4, Float4x4 }; };

        float*              GetFloat1Parameters()                       { return AsPointer(_float1Parameters.begin());         }
        Float3*             GetFloat3Parameters()                       { return AsPointer(_float3Parameters.begin());         }
        Float4*             GetFloat4Parameters()                       { return AsPointer(_float4Parameters.begin());         }
        Float4x4*           GetFloat4x4Parameters()                     { return AsPointer(_float4x4Parameters.begin());       }

        const float*        GetFloat1Parameters() const                 { return AsPointer(_float1Parameters.begin());         }
        const Float3*       GetFloat3Parameters() const                 { return AsPointer(_float3Parameters.begin());         }
        const Float4*       GetFloat4Parameters() const                 { return AsPointer(_float4Parameters.begin());         }
        const Float4x4*     GetFloat4x4Parameters() const               { return AsPointer(_float4x4Parameters.begin());       }

        size_t              GetFloat1ParametersCount() const            { return _float1Parameters.size();    }
        size_t              GetFloat3ParametersCount() const            { return _float3Parameters.size();    }
        size_t              GetFloat4ParametersCount() const            { return _float4Parameters.size();    }
        size_t              GetFloat4x4ParametersCount() const          { return _float4x4Parameters.size();  }

        SerializableVector<Float4x4>&   GetFloat4x4ParametersVector()   { return _float4x4Parameters; }
        SerializableVector<Float4>&     GetFloat4ParametersVector()     { return _float4Parameters; }
        SerializableVector<Float3>&     GetFloat3ParametersVector()     { return _float3Parameters; }
        SerializableVector<float>&      GetFloat1ParametersVector()     { return _float1Parameters; }
            
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

    inline Float3 AsFloat3(const float input[])     { return Float3(input[0], input[1], input[2]); }

    template<typename IteratorType>
        void GenerateOutputTransformsFree(
            Float4x4                            result[],
            size_t                              resultCount,
            const TransformationParameterSet*   parameterSet,
            const uint32*                       commandStreamBegin,
            const uint32*                       commandStreamEnd,
            IteratorType                        debugIterator,
            const void*                         iteratorUserData)
    {
            //
            //      Follow the commands in our command list, and output
            //      the resulting transformations.
            //

        Float4x4 workingStack[64]; // (fairly large space on the stack)
        Float4x4* workingTransform = workingStack;
        *workingTransform = Identity<Float4x4>();

        const float*    float1s = nullptr;
        const Float3*   float3s = nullptr;
        const Float4*   float4s = nullptr;
        const Float4x4* float4x4s = nullptr;
        size_t    float1Count, float3Count, float4Count, float4x4Count;
        if (parameterSet) {
            float1s         = parameterSet->GetFloat1Parameters();
            float3s         = parameterSet->GetFloat3Parameters();
            float4s         = parameterSet->GetFloat4Parameters();
            float4x4s       = parameterSet->GetFloat4x4Parameters();
            float1Count     = parameterSet->GetFloat1ParametersCount();
            float3Count     = parameterSet->GetFloat3ParametersCount();
            float4Count     = parameterSet->GetFloat4ParametersCount();
            float4x4Count   = parameterSet->GetFloat4x4ParametersCount();
        } else {
            float1Count = float3Count = float4Count = float4x4Count = 0;
        }
        (void)float1s; (void)float3s; (void)float4s; (void)float4x4s;

        for (auto i=commandStreamBegin; i!=commandStreamEnd;) {
            auto commandIndex = *i++;
            switch (commandIndex) {
            case TransformStackCommand::PushLocalToWorld:
                if ((workingTransform+1) >= &workingStack[dimof(workingStack)]) {
                    Throw(::Exceptions::BasicLabel("Exceeded maximum stack depth in GenerateOutputTransforms"));
                }
                    
                if (workingTransform != workingStack) {
                    debugIterator(*(workingTransform-1), *workingTransform, iteratorUserData);
                }

                *(workingTransform+1) = *workingTransform;
                ++workingTransform;
                break;

            case TransformStackCommand::PopLocalToWorld:
                {
                    auto popCount = *i++;
                    if (workingTransform < workingStack+popCount) {
                        Throw(::Exceptions::BasicLabel("Stack underflow in GenerateOutputTransforms"));
                    }

                    workingTransform -= popCount;
                }
                break;

            case TransformStackCommand::TransformFloat4x4_Static:
                    //
                    //      Parameter is a static single precision 4x4 matrix
                    //
                {
                    // i = AdvanceTo16ByteAlignment(i);
                    const Float4x4& transformMatrix = *reinterpret_cast<const Float4x4*>(AsPointer(i)); 
                    i += 16;
                    *workingTransform = Combine(transformMatrix, *workingTransform);
                }
                break;

            case TransformStackCommand::Translate_Static:
                // i = AdvanceTo16ByteAlignment(i);
                Combine_InPlace(AsFloat3(reinterpret_cast<const float*>(AsPointer(i))), *workingTransform);
                i += 3;
                break;

            case TransformStackCommand::RotateX_Static:
                Combine_InPlace(RotationX(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::RotateY_Static:
                Combine_InPlace(RotationY(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::RotateZ_Static:
                Combine_InPlace(RotationZ(Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::Rotate_Static:
                // i = AdvanceTo16ByteAlignment(i);
                *workingTransform = Combine(MakeRotationMatrix(AsFloat3(reinterpret_cast<const float*>(AsPointer(i))), Deg2Rad(*reinterpret_cast<const float*>(AsPointer(i+3)))), *workingTransform);
                i += 4;
                break;

            case TransformStackCommand::UniformScale_Static:
                Combine_InPlace(UniformScale(*reinterpret_cast<const float*>(AsPointer(i))), *workingTransform);
                i++;
                break;

            case TransformStackCommand::ArbitraryScale_Static:
                Combine_InPlace(ArbitraryScale(AsFloat3(reinterpret_cast<const float*>(AsPointer(i)))), *workingTransform);
                i+=3;
                break;

            case TransformStackCommand::TransformFloat4x4_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float4x4Count) {
                        *workingTransform = Combine(float4x4s[parameterIndex], *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for TransformFloat4x4_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::Translate_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float3Count) {
                        Combine_InPlace(float3s[parameterIndex], *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for Translate_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateX_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationX(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateX_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateY_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationY(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateY_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::RotateZ_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(RotationZ(Deg2Rad(float1s[parameterIndex])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for RotateZ_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::Rotate_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float4Count) {
                        *workingTransform = Combine(MakeRotationMatrix(Truncate(float4s[parameterIndex]), Deg2Rad(float4s[parameterIndex][3])), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for Rotate_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::UniformScale_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float1Count) {
                        Combine_InPlace(UniformScale(float1s[parameterIndex]), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for UniformScale_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::ArbitraryScale_Parameter:
                {
                    uint32 parameterIndex = *i++;
                    if (parameterIndex < float3Count) {
                        Combine_InPlace(ArbitraryScale(float3s[parameterIndex]), *workingTransform);
                    } else {
                        LogWarning << "Warning -- bad parameter index for ArbitraryScale_Parameter command (" << parameterIndex << ")";
                    }
                }
                break;

            case TransformStackCommand::WriteOutputMatrix:
                    //
                    //      Dump the current working transform to the output array
                    //
                {
                    uint32 outputIndex = *i++;
                    if (outputIndex < resultCount) {
                        result[outputIndex] = *workingTransform;

                        if (workingTransform != workingStack) {
                            debugIterator(*(workingTransform-1), *workingTransform, iteratorUserData);
                        }
                    } else {
                        LogWarning << "Warning -- bad output matrix index (" << outputIndex << ")";
                    }
                }
                break;
            }
        }
    }

    void TraceTransformationMachine(
            Utility::OutputStream&      outputStream,
            const uint32*               commandStreamBegin,
            const uint32*               commandStreamEnd);
}}

