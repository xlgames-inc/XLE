// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "STransformationMachine.h"
#include "SkeletonRegistry.h"
#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"            // just for AsString
#include "../../RenderCore/Assets/NascentTransformationMachine.h"
#include "../../ConsoleRig/Log.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"


namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    //#pragma warning(disable:4505) // 'RenderCore::ColladaConversion::FloatBits' : unreferenced local function has been removed)
    static unsigned int FloatBits(float input)
    {
            // (or just use a reinterpret cast)
        union Converter { float f; unsigned int i; };
        Converter c; c.f = float(input); 
        return c.i;
    }
    //
    //static unsigned int FloatBits(double input)
    //{
    //        // (or just use a reinterpret cast)
    //    union Converter { float f; unsigned int i; };
    //    Converter c; c.f = float(input); 
    //    return c.i;
    //}

    unsigned PushTransformations(
        RenderCore::Assets::NascentTransformationMachine& dst,
        const Transformation& transformations,
        const char nodeName[],
        const SkeletonRegistry& nodeRefs, bool assumeEverythingAnimated)
    {
        dst.ResolvePendingPops();

        if (!transformations)
            return 0;

            //
            //      Push in the commands for this node
            //

        dst.PushCommand(Assets::TransformStackCommand::PushLocalToWorld);
        const unsigned pushCount = 1;

        #if defined(_DEBUG)
            unsigned typesOfEachTransform[7] = {0,0,0,0,0,0};
        #endif

            //
            //      First, push in the transformations information.
            //      We're going to push in just the raw data from Collada
            //      This is most useful for animating stuff; because we
            //      can just change the parameters exactly as they appear
            //      in the raw data stream.
            //

        Transformation trans = transformations;
        for (; trans; trans = trans.GetNext()) {
            auto type = trans.GetType();
            if (type == TransformationSet::Type::None) continue;

            #if defined(_DEBUG)
                unsigned typeIndex = 0;
                if (unsigned(type) < dimof(typesOfEachTransform)) {
                    typeIndex = typesOfEachTransform[(unsigned)type];
                    ++typesOfEachTransform[(unsigned)type];
                }
            #endif

                //
                //      Sometimes the transformation is static -- and it's better
                //      to combine multiple transforms into one. 
                //
                //      However, we should do this after the full transformation
                //      stream has been made. That way we can use the same logic
                //      to combine transformations from multiple nodes into one.
                //

            enum ParameterType
            {
                ParameterType_Embedded,
                ParameterType_AnimationConstant,
                ParameterType_Animated
            } parameterType;

            auto paramName = std::string(nodeName) + "/" + AsString(trans.GetSid());
            const bool isAnimated = assumeEverythingAnimated || nodeRefs.IsAnimated(paramName);
            parameterType = isAnimated ? ParameterType_Animated : ParameterType_Embedded;
            auto parameterId = Hash32(AsPointer(paramName.cbegin()), AsPointer(paramName.cend()));

            if  (type == TransformationSet::Type::Matrix4x4) {

                    //
                    //      Do we need 128 bit alignment for this matrix?
                    //
                uint32 paramIndex = ~0u;
                if (    parameterType != ParameterType_Embedded
                    &&  dst.TryAddParameter(paramIndex, *(const Float4x4*)trans.GetUnionData(), parameterId, paramName.c_str())) {

                    dst.PushCommand(Assets::TransformStackCommand::TransformFloat4x4_Parameter);
                    dst.PushCommand(paramIndex);
                } else {
                    if (Equivalent(*(Float4x4*)trans.GetUnionData(), Identity<Float4x4>(), 1e-5f)) {
                        // ignore transform by identity
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::TransformFloat4x4_Static);
                        dst.PushCommand(trans.GetUnionData(), sizeof(Float4x4));
                    }
                }

            } else if (type == TransformationSet::Type::Translate) {

                uint32 paramIndex = ~0u;
                if (    parameterType != ParameterType_Embedded
                    &&  dst.TryAddParameter(paramIndex, *(const Float3*)trans.GetUnionData(), parameterId, paramName.c_str())) {

                    dst.PushCommand(Assets::TransformStackCommand::Translate_Parameter);
                    dst.PushCommand(paramIndex);
                } else {
                    if (Equivalent(*(Float3*)trans.GetUnionData(), Float3(0.f, 0.f, 0.f), 1e-5f)) {
                        // ignore translate by zero
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::Translate_Static);
                        dst.PushCommand(trans.GetUnionData(), sizeof(Float3));
                    }
                }

            } else if (type == TransformationSet::Type::Rotate) {

                const auto& rot = *(const ArbitraryRotation*)trans.GetUnionData();
                uint32 paramIndex = ~0u;
                if (    parameterType != ParameterType_Embedded
                    &&  dst.TryAddParameter(paramIndex, *(const Float4*)&rot, parameterId, paramName.c_str())) {

                        // Post animation, this may become a rotation around any axis. So
                        // we can't perform an optimisation to squish it to rotation around
                        // one of the cardinal axes
                    dst.PushCommand(Assets::TransformStackCommand::Rotate_Parameter);
                    dst.PushCommand(paramIndex);

                } else {

                    if (Equivalent(rot._angle, 0.f, 1e-5f)) {
                        // the angle is too small -- just ignore it
                    } else if (signed x = rot.IsRotationX()) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateX_Static);
                        dst.PushCommand(FloatBits(float(x) * rot._angle));
                    } else if (signed y = rot.IsRotationY()) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateY_Static);
                        dst.PushCommand(FloatBits(float(y) * rot._angle));
                    } else if (signed z = rot.IsRotationZ()) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateZ_Static);
                        dst.PushCommand(FloatBits(float(z) * rot._angle));
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::Rotate_Static);
                        dst.PushCommand(&rot, sizeof(rot));
                    }

                }
                        
            } else if (type == TransformationSet::Type::Scale) {

                    //
                    //      If the scale values start out uniform, let's assume
                    //      they stay uniform over all animations.
                    //
                    //      We can't guarantee that case. For example, and object
                    //      may start with (1,1,1) scale, and change to (2,1,1)
                    //
                    //      But, let's just ignore that possibility for the moment.
                    //
                auto scale = *(const Float3*)trans.GetUnionData();
                bool isUniform = Equivalent(scale[0], scale[1], 0.001f) && Equivalent(scale[0], scale[2], 0.001f);
                bool writeEmbedded = true;

                if (parameterType != ParameterType_Embedded) {
                    uint32 paramIndex = ~0u;
                    if (isUniform) {
                        if (dst.TryAddParameter(paramIndex, scale[0], parameterId, paramName.c_str())) {
                            dst.PushCommand(Assets::TransformStackCommand::UniformScale_Parameter);
                            dst.PushCommand(paramIndex);
                            writeEmbedded = false;
                        }
                    } else {
                        if (dst.TryAddParameter(paramIndex, scale, parameterId, paramName.c_str())) {
                            dst.PushCommand(Assets::TransformStackCommand::ArbitraryScale_Parameter);
                            dst.PushCommand(paramIndex);
                            writeEmbedded = false;
                        }
                    }
                }

                if (writeEmbedded) {
                    if (Equivalent(scale, Float3(1.f, 1.f, 1.f), 1e-5f)) {
                        // scaling by 1 -- just ignore
                    } else if (isUniform) {
                        dst.PushCommand(Assets::TransformStackCommand::UniformScale_Static);
                        dst.PushCommand(FloatBits(scale[0]));
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::ArbitraryScale_Static);
                        dst.PushCommand(&scale, sizeof(scale));
                    }
                }

            } else {

                LogAlwaysWarningF(
                    "Warning -- unsupported transformation type found in node (%s) -- type (%i)\n", 
                    nodeName, (unsigned)(type));

            }
        }

        return pushCount;
    }




}}


