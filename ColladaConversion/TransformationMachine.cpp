// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TransformationMachine.h"
#include "ColladaUtils.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"

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

    unsigned    NascentTransformationMachine_Collada::PushTransformations(
                        const COLLADAFW::TransformationPointerArray& transformations,
                        const char nodeName[])
    {
        if (_pendingPops) {
            _commandStream.push_back(Assets::TransformStackCommand::PopLocalToWorld);
            _commandStream.push_back(_pendingPops);
            _pendingPops = 0;
        }

        if (transformations.empty())
            return 0;

            //
            //      Push in the commands for this node
            //

        using namespace COLLADAFW;
        _commandStream.push_back(Assets::TransformStackCommand::PushLocalToWorld);
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
                parameterHash = AsHashedColladaUniqueId(transformations[c]->getAnimationList());
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
                    ThrowException(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }

                    //
                    //      Do we need 128 bit alignment for this matrix?
                    //
                if (parameterType == ParameterType_Embedded) {
                    _commandStream.push_back(Assets::TransformStackCommand::TransformFloat4x4_Static);
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[0][0]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[0][1]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[0][2]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[0][3]));

                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[1][0]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[1][1]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[1][2]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[1][3]));

                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[2][0]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[2][1]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[2][2]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[2][3]));

                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[3][0]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[3][1]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[3][2]));
                    _commandStream.push_back(FloatBits(matrixTransform->getMatrix()[3][3]));
                } else {
                    _commandStream.push_back(Assets::TransformStackCommand::TransformFloat4x4_Parameter);
                    _commandStream.push_back(AddParameter(AsFloat4x4(matrixTransform->getMatrix()), parameterHash, nodeName));
                }

            } else if (type == Transformation::TRANSLATE) {

                const Translate* translation = (const Translate*)(transformations[c]);
                if (!translation) {
                    ThrowException(FormatError(
                        "Bad transformation list in node (%s)", nodeName));
                }

                if (parameterType == ParameterType_Embedded) {
                    _commandStream.push_back(Assets::TransformStackCommand::Translate_Static);
                    _commandStream.push_back(FloatBits(translation->getTranslation().x));
                    _commandStream.push_back(FloatBits(translation->getTranslation().y));
                    _commandStream.push_back(FloatBits(translation->getTranslation().z));
                } else {
                    _commandStream.push_back(Assets::TransformStackCommand::Translate_Parameter);
                    _commandStream.push_back(AddParameter(AsFloat3(translation->getTranslation()), parameterHash, nodeName));
                }

            } else if (type == Transformation::ROTATE) {

                const Rotate* rotation = (const Rotate*)(transformations[c]);
                if (!rotation) {
                    ThrowException(FormatError(
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
                        _commandStream.push_back(Assets::TransformStackCommand::RotateX_Static);
                        _commandStream.push_back(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(-1.f, 0.f, 0.f)) {
                        _commandStream.push_back(Assets::TransformStackCommand::RotateX_Static);
                        _commandStream.push_back(FloatBits(-rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 1.f, 0.f)) {
                        _commandStream.push_back(Assets::TransformStackCommand::RotateY_Static);
                        _commandStream.push_back(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, -1.f, 0.f)) {
                        _commandStream.push_back(Assets::TransformStackCommand::RotateY_Static);
                        _commandStream.push_back(FloatBits(-rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, 1.f)) {
                        _commandStream.push_back(Assets::TransformStackCommand::RotateZ_Static);
                        _commandStream.push_back(FloatBits(rotation->getRotationAngle()));
                    } else if (rotation->getRotationAxis() == COLLADABU::Math::Vector3(0.f, 0.f, -1.f)) {
                        _commandStream.push_back(Assets::TransformStackCommand::RotateZ_Static);
                        _commandStream.push_back(FloatBits(-rotation->getRotationAngle()));
                    } else {
                        _commandStream.push_back(Assets::TransformStackCommand::Rotate_Static);
                        _commandStream.push_back(FloatBits(rotation->getRotationAxis().x));
                        _commandStream.push_back(FloatBits(rotation->getRotationAxis().y));
                        _commandStream.push_back(FloatBits(rotation->getRotationAxis().z));
                        _commandStream.push_back(FloatBits(rotation->getRotationAngle()));
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

                    _commandStream.push_back(type);
                    if (type == Assets::TransformStackCommand::Rotate_Parameter) {
                        const Float4 rotationAxisAndAngle = Expand(AsFloat3(rotation->getRotationAxis()), (float)rotation->getRotationAngle());
                        _commandStream.push_back(AddParameter(rotationAxisAndAngle, parameterHash, nodeName));
                    } else {
                        _commandStream.push_back(AddParameter(float(rotation->getRotationAngle()), parameterHash, nodeName));
                    }
                }
                        
            } else if (type == Transformation::SCALE) {

                const Scale* scale = (const Scale*)(transformations[c]);
                if (!scale) {
                    ThrowException(FormatError(
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
                        _commandStream.push_back(Assets::TransformStackCommand::UniformScale_Static);
                        _commandStream.push_back(FloatBits(scale->getScale().x));
                    } else {
                        _commandStream.push_back(Assets::TransformStackCommand::ArbitraryScale_Static);
                        _commandStream.push_back(FloatBits(scale->getScale().x));
                        _commandStream.push_back(FloatBits(scale->getScale().y));
                        _commandStream.push_back(FloatBits(scale->getScale().z));
                    }
                } else {
                    if (    Equivalent(scale->getScale().x, scale->getScale().y, COLLADABU::Math::Real(0.001))
                        &&  Equivalent(scale->getScale().x, scale->getScale().z, COLLADABU::Math::Real(0.001))) {
                        _commandStream.push_back(Assets::TransformStackCommand::UniformScale_Parameter);
                        _commandStream.push_back(AddParameter(float(scale->getScale().x), parameterHash, nodeName));
                    } else {
                        _commandStream.push_back(Assets::TransformStackCommand::ArbitraryScale_Parameter);
                        _commandStream.push_back(AddParameter(AsFloat3(scale->getScale()), parameterHash, nodeName));
                    }
                }

            } else {

                Warning("Warning -- unsupported transformation type found in node (%s) -- type (%s)\n", nodeName, AsString(type));

            }
        }

        return pushCount;
    }


    NascentTransformationMachine_Collada::NascentTransformationMachine_Collada()
    {}

    NascentTransformationMachine_Collada::NascentTransformationMachine_Collada(NascentTransformationMachine_Collada&& machine)
    :   NascentTransformationMachine(std::forward<NascentTransformationMachine>(machine))
    {
    }
    
    NascentTransformationMachine_Collada& NascentTransformationMachine_Collada::operator=(NascentTransformationMachine_Collada&& moveFrom)
    {
        NascentTransformationMachine::operator=(NascentTransformationMachine(std::forward<NascentTransformationMachine>(moveFrom)));
        return *this;
    }

    NascentTransformationMachine_Collada::~NascentTransformationMachine_Collada()
    {}



}}


