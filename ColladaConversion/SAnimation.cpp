// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SAnimation.h"
#include "ConversionObjects.h"
#include "Scaffold.h"
#include "ParsingUtil.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;
    using ::Assets::Exceptions::FormatError;

    class SidBreakdown
    {
    public:
        Section _baseObject;
        std::vector<Section> _scoping;
        Section _fieldReference;

        SidBreakdown(Section base);
        SidBreakdown() {}
    };

    SidBreakdown::SidBreakdown(Section base)
    {
        const utf8 scopeSeparator = (utf8)'/';
        const utf8 fieldSeparator = (utf8)'.';

        const auto* start = base._start;
        const auto* i = base._start;
        while (i != base._end && *i != scopeSeparator && *i != fieldSeparator)
            ++i;
        _baseObject = Section(start, i);

        while (i != base._end) {
            if (*i == scopeSeparator) {
                ++i;
                start = i;
                while (i != base._end && *i != scopeSeparator && *i != fieldSeparator)
                    ++i;
                _scoping.push_back(Section(start, i));
            } else if (*i == fieldSeparator) {
                _fieldReference = Section(i+1, base._end);
                break;
            }
        }
    }

    std::string AsParameterName(SidBreakdown breakdown, const URIResolveContext& resolveContext)
    {
        // This is awkward because to want to convert from the node "id" to the node "name"
        // We have the node id in the breakdown. But to get the name, we need to find the
        // actual node, and extract the name.
        // We do this because we want the references to bones to be consistant between files.
        // While the "id" of the bone is unique across a single file, there is no real 
        // guaranteed that the bone will have the same id in all files. However, the name will
        // normally be closely associated with the real name of the object in the authoring
        // tool -- which means that we can except that it will be the same in different exports,
        // (assuming the original data hasn't been changed)
        std::string temp;
        auto ele = FindElement(GuidReference(breakdown._baseObject), resolveContext, &IDocScopeIdResolver::FindNode);
        if (ele) temp = AsString(ele.GetName());
        else temp = AsString(breakdown._baseObject);

        for (const auto&i:breakdown._scoping)
            temp += "/" + AsString(i);
        return temp;
    }

    class SimpleChannel
    {
    public:
        SidBreakdown            _target;
        const Sampler*          _sampler;
        const DataFlow::Source* _outputSource;
        const DataFlow::Source* _inputSource;
        const DataFlow::Source* _inTangentsSource;
        const DataFlow::Source* _outTangentsSource;
        bool _valid;

        SimpleChannel(const Channel& channel, const URIResolveContext& resolveContext);
    };

    SimpleChannel::SimpleChannel(const Channel& channel, const URIResolveContext& resolveContext)
    {
        _valid = false;
        _sampler = nullptr;
        _outputSource = _inputSource = nullptr;
        _inTangentsSource = _outTangentsSource = nullptr;

        _target = SidBreakdown(channel.GetTarget());

        _sampler = FindElement(channel.GetSource(), resolveContext, &IDocScopeIdResolver::FindSampler);
        if (!_sampler) {
            LogWarning << "Found animation channel with no sampler. Ignoring";
            return;
        }

        //////////////////
        const auto* output = _sampler->GetInputsCollection().FindInputBySemantic(u("OUTPUT"));
        if (!output) {
            LogWarning << "Found animation sampler with no output field. Ignoring";
            return;
        }

        _outputSource = FindElement(output->_source, resolveContext, &IDocScopeIdResolver::FindSource);
        if (!_outputSource) {
            LogWarning << "Could not find <source> for output of animation sampler. Ignoring";
            return;
        }

        if (!_outputSource->FindAccessorForTechnique()) {
            LogWarning << "<source> for output of animation sampler does not contain a sampler for common profile. Ignoring";
            return;
        }

        //////////////////
        const auto* input = _sampler->GetInputsCollection().FindInputBySemantic(u("INPUT"));
        if (!input) {
            LogWarning << "Found animation sampler with no output field. Ignoring";
            return;
        }

        _inputSource = FindElement(input->_source, resolveContext, &IDocScopeIdResolver::FindSource);
        if (!_inputSource) {
            LogWarning << "Could not find <source> for input of animation sampler. Ignoring";
            return;
        }

        if (!_inputSource->FindAccessorForTechnique()) {
            LogWarning << "<source> for input of animation sampler does not contain a sampler for common profile. Ignoring";
            return;
        }

        //////////////////
        const auto* inTangent = _sampler->GetInputsCollection().FindInputBySemantic(u("IN_TANGENT"));
        if (inTangent) {
            _inTangentsSource = FindElement(inTangent->_source, resolveContext, &IDocScopeIdResolver::FindSource);
            if (!_inTangentsSource) {
                LogWarning << "Could not find <source> for input tangent of animation sampler. Ignoring";
                return;
            }

            if (!_inTangentsSource->FindAccessorForTechnique()) {
                LogWarning << "<source> for input tangent of animation sampler does not contain a sampler for common profile. Ignoring";
                return;
            }
        }

        //////////////////
        const auto* outTangent = _sampler->GetInputsCollection().FindInputBySemantic(u("OUT_TANGENT"));
        if (outTangent) {
            _outTangentsSource = FindElement(outTangent->_source, resolveContext, &IDocScopeIdResolver::FindSource);
            if (!_outTangentsSource) {
                LogWarning << "Could not find <source> for output tangent of animation sampler. Ignoring";
                return;
            }

            if (!_outTangentsSource->FindAccessorForTechnique()) {
                LogWarning << "<source> for output tangent of animation sampler does not contain a sampler for common profile. Ignoring";
                return;
            }
        }

        //////////////////
        _valid = true;
    }

    // static ImpliedTyping::TypeDesc AsTypeDesc(Section section)
    // {
    //     return HLSLTypeNameAsTypeDesc(AsString(section).c_str());
    // }

    std::pair<const utf8*, unsigned> s_FieldNameToOffset[]
    {
        std::make_pair(u("X"), 0),
        std::make_pair(u("Y"), 1),
        std::make_pair(u("Z"), 2),
        std::make_pair(u("W"), 3),

        std::make_pair(u("x"), 0),
        std::make_pair(u("y"), 1),
        std::make_pair(u("z"), 2),
        std::make_pair(u("w"), 3),

        std::make_pair(u("S"), 0),
        std::make_pair(u("T"), 1),
        std::make_pair(u("Q"), 2),

        std::make_pair(u("s"), 0),
        std::make_pair(u("t"), 1),
        std::make_pair(u("q"), 2),

        std::make_pair(u("ANGLE"), 3),
        std::make_pair(u("angle"), 3)
    };

    static void LoadSource(
        float* dest, unsigned outputStride, unsigned outputEleCount, unsigned outputFields,
        const DataFlow::Source* src, unsigned srcSkip = 0)
    {
        unsigned c=0;
        unsigned fieldIterator = 0;
        const auto* end = src->GetArrayData()._end;
        const auto* i = src->GetArrayData()._start;
        while (c<outputEleCount) {
            while (i < end && IsWhitespace(*i)) ++i;
            if (i == end) break;

            dest[c*outputStride+fieldIterator] = 0.f;
            i = FastParseElement(dest[c*outputStride+fieldIterator], i, end);
            ++fieldIterator;
            if (fieldIterator >= outputFields) { ++c; fieldIterator=0; };
            
                // skip over elements they we are going to ignore
            for (unsigned sk=0; sk<srcSkip; ++sk) {
                while (i < end && IsWhitespace(*i)) ++i;
                while (i < end && !IsWhitespace(*i)) ++i;
            }
        }

        for (;c<outputEleCount; ++c)
            dest[c*outputStride] = 0.f;
    }

    UnboundAnimation Convert(
        const Animation& animation, 
        const URIResolveContext& resolveContext, 
        NodeReferences& nodeRefs)
    {
        UnboundAnimation result;

            // First we need to group the curves by the output
            // parameter. There can be separate curves for separate
            // "fields" in the same parameter (eg, X, Y, Z)
        std::vector<std::pair<std::string, unsigned>> orderedByParam;
        for (unsigned c=0; c<animation.GetChannelCount(); ++c) {
            const auto& channel = animation.GetChannel(c);
            SimpleChannel ch(channel, resolveContext);
            if (!ch._valid) continue;

            auto paramName = AsParameterName(ch._target, resolveContext);
            orderedByParam.insert(
                LowerBound(orderedByParam, paramName),
                std::make_pair(paramName, c));
        }

            // the breakdown should reference a parameter in the skeleton
            // If we haven't build the skeleton yet, we don't strictly
            // know what type the parameter actually is. We can only
            // make assumptions based on the curves we have available
        for (auto i=orderedByParam.cbegin(); i!=orderedByParam.cend();) {
            auto i2 = i+1;
            while (i2 != orderedByParam.cend() && i2->first == i->first) ++i2;

                // All of these curves refer to the same parameter. We need to
                // decide on the dimensionality of the output.
            unsigned outDimension = 0;
            for (auto i3=i; i3<i2; ++i3) {
                SimpleChannel ch(animation.GetChannel(i->second), resolveContext);
                assert(ch._valid);

                if (ch._target._fieldReference._end > ch._target._fieldReference._start) {
                    
                    auto* knownField = std::find_if(
                        s_FieldNameToOffset, &s_FieldNameToOffset[dimof(s_FieldNameToOffset)],
                        [&ch](const std::pair<const utf8*, unsigned>& p) 
                            { return Is(ch._target._fieldReference, p.first); });
                    if (knownField) {
                        outDimension = knownField->second + 1;
                    } else {
                        LogWarning << "Unknown field name " << AsString(ch._target._fieldReference) << " in animation curve target";
                    }

                } else {
                    outDimension = std::max(outDimension, ch._outputSource->FindAccessorForTechnique()->GetStride());
                }
            }

                // We need to combine all of these curves into a single one.
                // We're going to require that the input curves are "time" and
                // they all match exactly.

            SimpleChannel firstChannel(animation.GetChannel(i->second), resolveContext);
            auto keyCount = firstChannel._inputSource->FindAccessorForTechnique()->GetCount();

            using namespace Metal;
            NativeFormat::Enum  
                positionFormat    = NativeFormat::Unknown,
                inTangentFormat   = NativeFormat::Unknown,
                outTangentFormat  = NativeFormat::Unknown;

            Assets::TransformationParameterSet::Type::Enum samplerType;
            
            switch (outDimension) {
            case 1:     positionFormat = NativeFormat::R32_FLOAT; samplerType = Assets::TransformationParameterSet::Type::Float1; break;
            case 3:     positionFormat = NativeFormat::R32G32B32_FLOAT; samplerType = Assets::TransformationParameterSet::Type::Float3; break;
            case 4:     positionFormat = NativeFormat::R32G32B32A32_FLOAT; samplerType = Assets::TransformationParameterSet::Type::Float4; break;
            case 16:    positionFormat = NativeFormat::Matrix4x4; samplerType = Assets::TransformationParameterSet::Type::Float4x4; break;
            default:    Throw(FormatError("Out dimension in animation is invalid (%i). Expected 1, 3, 4 or 16.", outDimension));
            }

            if (firstChannel._inTangentsSource) inTangentFormat = positionFormat;
            if (firstChannel._outTangentsSource) outTangentFormat = positionFormat;

            const auto positionBytes = Metal::BitsPerPixel(positionFormat)/8;
            const auto inTangentBytes = Metal::BitsPerPixel(inTangentFormat)/8;
            const auto elementBytes = positionBytes + inTangentBytes+ Metal::BitsPerPixel(outTangentFormat)/8;

            auto keyBlock = std::unique_ptr<uint8[], Serialization::BlockSerializerDeleter<uint8[]>>(
                new uint8[elementBytes * keyCount]);
            auto inTangentsOffsetBytes = positionBytes;
            auto outTangentsOffsetBytes = inTangentsOffsetBytes + inTangentBytes;

                //  There's a problem with max exports that have rotations around cardinal axes
                //  This can result in skeleton transforms like <rotation>0 1 0 angle</rotation>
                //  We treat this as a single 4D parameter. But the animation curve might only be
                //  applied to the angle part. In these cases, we try to animate it as a 4D vector, 
                //  and the axis part gets wiped of it's (static) values.

            for (auto i3=i; i3<i2; ++i3) {
                SimpleChannel ch(animation.GetChannel(i->second), resolveContext);
                assert(ch._valid);

                unsigned fieldOffset = 0, fieldCount = 0;
                if (ch._target._fieldReference._end > ch._target._fieldReference._start) {
                    
                    auto* knownField = std::find_if(
                        s_FieldNameToOffset, &s_FieldNameToOffset[dimof(s_FieldNameToOffset)],
                        [&ch](const std::pair<const utf8*, unsigned>& p) 
                            { return Is(ch._target._fieldReference, p.first); });
                    if (knownField) {
                        fieldOffset = knownField->second;
                        fieldCount = 1;
                    }
                    
                } else {
                    fieldCount = std::max(outDimension, ch._outputSource->FindAccessorForTechnique()->GetStride());
                }

                if (!fieldCount) continue;

                LoadSource(
                    (float*)PtrAdd(keyBlock.get(), sizeof(float) * fieldOffset),
                    (unsigned)(elementBytes/sizeof(float)), keyCount, fieldCount, 
                    ch._outputSource);

                // Note that we're expecting the tangents to be in the same format and dimensions
                // as the positions.
                // Some exporters are using an XY format for the tangents (apparently to create a 
                // unit length vector in 2D space).. We might need to adjust the x part in these cases
                // to get back to the 1D tangent value we need for the bezier equation)

                if (ch._inTangentsSource) {
                    if (ch._inTangentsSource->FindAccessorForTechnique()->GetCount() > 1)
                        LogWarning << "Tangents expressed in XY format in animation curve. Ignoring Y part.";

                    LoadSource(
                        (float*)PtrAdd(keyBlock.get(), sizeof(float) * fieldOffset + inTangentsOffsetBytes),
                        (unsigned)(elementBytes/sizeof(float)), keyCount, fieldCount, 
                        ch._inTangentsSource, ch._inTangentsSource->FindAccessorForTechnique()->GetStride()-1);
                }

                if (ch._outTangentsSource) {
                    if (ch._outTangentsSource->FindAccessorForTechnique()->GetCount() > 1)
                        LogWarning << "Tangents expressed in XY format in animation curve. Ignoring Y part.";

                    LoadSource(
                        (float*)PtrAdd(keyBlock.get(), sizeof(float) * fieldOffset + outTangentsOffsetBytes),
                        (unsigned)(elementBytes/sizeof(float)), keyCount, fieldCount, 
                        ch._outTangentsSource, ch._outTangentsSource->FindAccessorForTechnique()->GetStride()-1);
                }
            }

                // note --  we're expecting all channels to have exactly the same "time"
                //          array. If they are not the same, it means the key frames for
                //          different fields are in different places. That would cause a lot
                //          of problems.
            auto inputTimeBlock = std::unique_ptr<float[], Serialization::BlockSerializerDeleter<float[]>>(new float[keyCount]);
            LoadSource(
                inputTimeBlock.get(),
                1, keyCount, 1, 
                firstChannel._inputSource, 
                firstChannel._inputSource->FindAccessorForTechnique()->GetStride()-1);

                // todo -- we need to find the correct animation curve type

            Assets::RawAnimationCurve curve(
                (size_t)keyCount, std::move(inputTimeBlock),
                DynamicArray<uint8, Serialization::BlockSerializerDeleter<uint8[]>>(
                    std::move(keyBlock), elementBytes * keyCount),
                elementBytes, Assets::RawAnimationCurve::Linear,
                positionFormat, inTangentFormat, outTangentFormat);
            result._curves.emplace_back(UnboundAnimation::Curve(
                i->first, std::move(curve), samplerType, 0));
            
            nodeRefs.MarkParameterAnimated(i->first);

            i = i2;
        }

        return std::move(result);
    }

}}

