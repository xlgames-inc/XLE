// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OCMisc.h"
#include "../ConversionUtil.h"
// #include "../NascentAnimController.h"
#include "../../RenderCore/Assets/NascentTransformationMachine.h"
#include "../../ConsoleRig/Log.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWVisualScene.h>
    #include <COLLADAFWLibraryNodes.h>
    #include <COLLADAFWTranslate.h>
    #include <COLLADAFWMatrix.h>
    #include <COLLADAFWRotate.h>
    #include <COLLADAFWScale.h>
#pragma warning(pop)



namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    static const char* AsString(COLLADAFW::Transformation::TransformationType type)
    {
        switch (type) {
        case COLLADAFW::Transformation::MATRIX:        return "Matrix";
        case COLLADAFW::Transformation::TRANSLATE:     return "Translate";
        case COLLADAFW::Transformation::ROTATE:        return "Rotate";
        case COLLADAFW::Transformation::SCALE:         return "Scale";
        case COLLADAFW::Transformation::LOOKAT:        return "Lookat";
        case COLLADAFW::Transformation::SKEW:          return "Skew";
        }
        return "<<unknown>>";
    }

    /*static const char* AsString(COLLADAFW::Transformation::TransformationType type)
    {
        switch (type) {
        case COLLADAFW::Transformation::MATRIX:    return "matrix";
        case COLLADAFW::Transformation::TRANSLATE: return "translate";
        case COLLADAFW::Transformation::ROTATE:    return "rotate";
        case COLLADAFW::Transformation::SCALE:     return "scale";
        case COLLADAFW::Transformation::LOOKAT:    return "lookat";
        case COLLADAFW::Transformation::SKEW:      return "skew";
        default: return "<<unknown>>";
        }
    }*/

    unsigned    PushTransformations(
        RenderCore::Assets::NascentTransformationMachine& dst,
        const COLLADAFW::TransformationPointerArray& transformations,
        const char nodeName[])
    {
        dst.ResolvePendingPops();

        if (transformations.empty())
            return 0;

            //
            //      Push in the commands for this node
            //

        using namespace COLLADAFW;
        dst.PushCommand(Assets::TransformStackCommand::PushLocalToWorld);
        const unsigned pushCount = 1;

        unsigned typesOfEachTransform[6] = {0,0,0,0,0,0};

            //
            //      First, push in the transformations information.
            //      We're going to push in just the raw data from Collada
            //      This is most useful for animating stuff; because we
            //      can just change the parameters exactly as they appear
            //      in the raw data stream.
            //

        for (size_t c=0; c<transformations.getCount(); ++c) {
            const Transformation::TransformationType type = 
                transformations[c]->getTransformationType();

            unsigned typeIndex = 0;
            if (type < dimof(typesOfEachTransform)) {
                typeIndex = typesOfEachTransform[type];
                ++typesOfEachTransform[type];
            }

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

            uint32 parameterHash = 0;
            #pragma warning(disable:4127)       // C4127: conditional expression is constant
            if (!nodeName || nodeName[0] == '\0' || XlFindStringI(nodeName, "rope")!=nullptr) {
                parameterType = ParameterType_Embedded; // un-animatable embedded constant
            } else if (transformations[c]->getAnimationList() != UniqueId::INVALID) {
                parameterType = ParameterType_Animated;
                parameterHash = BuildAnimParameterId(transformations[c]->getAnimationList());
            } else {

                    //
                    //      Some constants must be associated with the
                    //      animation; but others can be associated with 
                    //      the transformation machine.
                    //
                    //      It's not always clear which is which... But use
                    //      "ParameterType_Embedded" for a constant value
                    //      that is associated with the transformation machine.
                    //

                parameterType = ParameterType_AnimationConstant;

                char buffer[256];
                xl_snprintf(buffer, dimof(buffer), "%s_%s(%i)", nodeName, AsString(type), typeIndex);
                parameterHash = Hash32(buffer, &buffer[XlStringLen(buffer)]);
            }

            if  (type == Transformation::MATRIX) {

                const Matrix* matrixTransform = (const Matrix*)(transformations[c]);
                if (!matrixTransform) {
                    Throw(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }

                    //
                    //      Do we need 128 bit alignment for this matrix?
                    //
                if (parameterType == ParameterType_Embedded) {
                    dst.PushCommand(Assets::TransformStackCommand::TransformFloat4x4_Static);
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[0][0]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[0][1]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[0][2]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[0][3]));

                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[1][0]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[1][1]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[1][2]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[1][3]));

                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[2][0]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[2][1]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[2][2]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[2][3]));

                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[3][0]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[3][1]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[3][2]));
                    dst.PushCommand(FloatBits(matrixTransform->getMatrix()[3][3]));
                } else {
                    dst.PushCommand(Assets::TransformStackCommand::TransformFloat4x4_Parameter);
                    dst.PushCommand(dst.AddParameter(AsFloat4x4(matrixTransform->getMatrix()), parameterHash, nodeName));
                }

            } else if (type == Transformation::TRANSLATE) {

                const Translate* translation = (const Translate*)(transformations[c]);
                if (!translation) {
                    Throw(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }

                if (parameterType == ParameterType_Embedded) {
                    dst.PushCommand(Assets::TransformStackCommand::Translate_Static);
                    dst.PushCommand(FloatBits(translation->getTranslation().x));
                    dst.PushCommand(FloatBits(translation->getTranslation().y));
                    dst.PushCommand(FloatBits(translation->getTranslation().z));
                } else {
                    dst.PushCommand(Assets::TransformStackCommand::Translate_Parameter);
                    dst.PushCommand(dst.AddParameter(AsFloat3(translation->getTranslation()), parameterHash, nodeName));
                }

            } else if (type == Transformation::ROTATE) {

                const Rotate* rotation = (const Rotate*)(transformations[c]);
                if (!rotation) {
                    Throw(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }

                    //
                    //      Note -- bitwise comparison for rotation axis
                    //              Given that we're only comparing with 0.f and 1.f,
                    //              the bitwise check should be fairly reliable. But we
                    //              do risk missing axes that are very close to the basic
                    //              axes, but not quite.
                    //
                    //              Assuming that our rotation rules are the same as
                    //              the Collada standard.
                    //
                    //          Assuming that the "axis" won't change under animation
                    //
                if (parameterType == ParameterType_Embedded) {
                    if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(1.f, 0.f, 0.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateX_Static);
                        dst.PushCommand(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(-1.f, 0.f, 0.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateX_Static);
                        dst.PushCommand(FloatBits(-rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 1.f, 0.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateY_Static);
                        dst.PushCommand(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, -1.f, 0.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateY_Static);
                        dst.PushCommand(FloatBits(-rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, 1.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateZ_Static);
                        dst.PushCommand(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, -1.f)) {
                        dst.PushCommand(Assets::TransformStackCommand::RotateZ_Static);
                        dst.PushCommand(FloatBits(-rotation->getRotationAngle()));
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::Rotate_Static);
                        dst.PushCommand(FloatBits(rotation->getRotationAxis().x));
                        dst.PushCommand(FloatBits(rotation->getRotationAxis().y));
                        dst.PushCommand(FloatBits(rotation->getRotationAxis().z));
                        dst.PushCommand(FloatBits(rotation->getRotationAngle()));
                    }
                } else {

                    Assets::TransformStackCommand::Enum type = Assets::TransformStackCommand::Rotate_Parameter;
                    if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(1.f, 0.f, 0.f)) {
                        type = Assets::TransformStackCommand::RotateX_Parameter;
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(-1.f, 0.f, 0.f)) {
                        type = Assets::TransformStackCommand::RotateX_Parameter;
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 1.f, 0.f)) {
                        type = Assets::TransformStackCommand::RotateY_Parameter;
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, -1.f, 0.f)) {
                        type = Assets::TransformStackCommand::RotateY_Parameter;
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, 1.f)) {
                        type = Assets::TransformStackCommand::RotateZ_Parameter;
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, -1.f)) {
                        type = Assets::TransformStackCommand::RotateZ_Parameter;
                    }

                    dst.PushCommand(type);
                    if (type == Assets::TransformStackCommand::Rotate_Parameter) {
                        const Float4 rotationAxisAndAngle = Expand(AsFloat3(rotation->getRotationAxis()), (float)rotation->getRotationAngle());
                        dst.PushCommand(dst.AddParameter(rotationAxisAndAngle, parameterHash, nodeName));
                    } else {
                        dst.PushCommand(dst.AddParameter(float(rotation->getRotationAngle()), parameterHash, nodeName));
                    }
                }
                        
            } else if (type == Transformation::SCALE) {

                const Scale* scale = (const Scale*)(transformations[c]);
                if (!scale) {
                    Throw(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }
                            
                    //
                    //      If the scale values start out uniform, let's assume
                    //      they stay uniform over all animations.
                    //
                    //      We can't guarantee that case. For example, and object
                    //      may start with (1,1,1) scale, and change to (2,1,1)
                    //
                    //      But, let's just ignore that possibility for the moment.
                    //
                if (parameterType == ParameterType_Embedded) {
                    if (    Equivalent(scale->getScale().x, scale->getScale().y, COLLADABU::Math::Real(0.001))
                        &&  Equivalent(scale->getScale().x, scale->getScale().z, COLLADABU::Math::Real(0.001))) {
                        dst.PushCommand(Assets::TransformStackCommand::UniformScale_Static);
                        dst.PushCommand(FloatBits(scale->getScale().x));
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::ArbitraryScale_Static);
                        dst.PushCommand(FloatBits(scale->getScale().x));
                        dst.PushCommand(FloatBits(scale->getScale().y));
                        dst.PushCommand(FloatBits(scale->getScale().z));
                    }
                } else {
                    if (    Equivalent(scale->getScale().x, scale->getScale().y, COLLADABU::Math::Real(0.001))
                        &&  Equivalent(scale->getScale().x, scale->getScale().z, COLLADABU::Math::Real(0.001))) {
                        dst.PushCommand(Assets::TransformStackCommand::UniformScale_Parameter);
                        dst.PushCommand(dst.AddParameter(float(scale->getScale().x), parameterHash, nodeName));
                    } else {
                        dst.PushCommand(Assets::TransformStackCommand::ArbitraryScale_Parameter);
                        dst.PushCommand(dst.AddParameter(AsFloat3(scale->getScale()), parameterHash, nodeName));
                    }
                }

            } else {

                LogAlwaysWarningF("Warning -- unsupported transformation type found in node (%s) -- type (%s)\n", nodeName, AsString(type));

            }
        }

        return pushCount;
    }




}}


