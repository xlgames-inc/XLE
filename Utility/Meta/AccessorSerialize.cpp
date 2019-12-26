// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AccessorSerialize.h"
#include "ClassAccessors.h"
#include "ClassAccessorsImpl.h"

#include "../../ConsoleRig/Log.h"
#include "../Streams/StreamFormatter.h"
#include "../StringFormat.h"
#include "../ParameterBox.h"
#include "../MemoryUtils.h"
#include "../Conversion.h"

namespace Utility
{
    static const unsigned ParsingBufferSize = 256;

    template<typename Formatter>
        void AccessorDeserialize(
            Formatter& formatter,
            void* obj, const ClassAccessors& props)
    {
        using Blob = typename Formatter::Blob;
        using CharType = typename Formatter::value_type;
        auto charTypeCat = ImpliedTyping::TypeOf<CharType>()._type;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    typename Formatter::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in begin element", formatter.GetLocation()));
                    
                    auto arrayBracket = std::find(name._start, name._end, '[');
                    if (arrayBracket == name._end) {
                        if (!props.TryOpaqueSet(
                            obj,
                            Hash64(name._start, name._end), MakeIteratorRange(value.begin(), value.end()), 
                            ImpliedTyping::TypeDesc(charTypeCat, uint16(value._end - value._start)), true)) {

                            Log(Warning) << "Failure while assigning property during deserialization -- " <<
                                Conversion::Convert<std::string>(std::basic_string<CharType>(name._start, name._end)) << std::endl;
                        }
                    } else {
                        auto arrayIndex = XlAtoUI32((const char*)(arrayBracket+1));
                        if (!props.TryOpaqueSet(
                            obj, Hash64(name._start, arrayBracket), arrayIndex, MakeIteratorRange(value.begin(), value.end()), 
                            ImpliedTyping::TypeDesc(charTypeCat, uint16(value._end - value._start)), true)) {

                            Log(Warning) << "Failure while assigning array property during deserialization -- " <<
                                Conversion::Convert<std::string>(std::basic_string<CharType>(name._start, name._end)) << std::endl;
                        }
                    }
                }
                break;
                    
            case Blob::AttributeValue:
            case Blob::CharacterData:
                assert(0);
                break;

            case Blob::EndElement:
            case Blob::None:
                return;

            case Blob::BeginElement:
                {
                    typename Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName))
                        Throw(FormatException("Error in begin element", formatter.GetLocation()));

                    auto created = props.TryCreateChild(obj, Hash64(eleName._start, eleName._end));
                    if (created.first) {
                        AccessorDeserialize(formatter, created.first, *created.second);
                    } else {
                        Log(Warning) << "Couldn't find a match for element name during deserialization -- " <<
                            Conversion::Convert<std::string>(std::basic_string<CharType>(eleName._start, eleName._end)) << std::endl;
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    break;
                }
            }
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void AccessorSerialize(
        OutputStreamFormatter& formatter,
        const void* obj, const ClassAccessors& props)
    {
        using CharType = utf8;
        auto charTypeCat = ImpliedTyping::TypeOf<CharType>()._type;
        CharType buffer[ParsingBufferSize];

        for (size_t i=0; i<props.GetPropertyCount(); ++i) {
            const auto& p = props.GetPropertyByIndex(i);
            if (p._castTo) {
                p._castTo(
                    obj, buffer, sizeof(buffer), 
                    ImpliedTyping::TypeDesc(charTypeCat, dimof(buffer)), true);

                formatter.WriteAttribute(
                    (const utf8*)AsPointer(p._name.cbegin()), (const utf8*)AsPointer(p._name.cend()),
                    buffer, XlStringEnd(buffer));
            }

            if (p._castToArray) {
                for (size_t e=0; e<p._fixedArrayLength; ++e) {
                    p._castToArray(
                        obj, e, buffer, sizeof(buffer), 
                        ImpliedTyping::TypeDesc(charTypeCat, dimof(buffer)), true);

                    StringMeld<256, CharType> name;
                    name << p._name.c_str() << "[" << e << "]";
                    formatter.WriteAttribute(name.get(), buffer);
                }
            }
        }

        for (size_t i=0; i<props.GetChildListCount(); ++i) {
            const auto& childList = props.GetChildListByIndex(i);
            auto count = childList._getCount(obj);
            for (size_t e=0; e<count; ++e) {
                const auto* child = childList._getByIndex(obj, e);
                auto eleId = formatter.BeginElement(childList._name);
                AccessorSerialize(formatter, child, *childList._childProps);
                formatter.EndElement(eleId);
            }
        }
    }

    template
        void AccessorDeserialize(
            InputStreamFormatter<utf8>& formatter,
            void* obj, const ClassAccessors& props);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void SetParameters(
        void* obj, const ClassAccessors& accessors,
        const ParameterBox& paramBox)
    {
        // we can choose to iterate through the parameters in either way:
        // either by iterating through the accessors in "accessors" and pulling
        // values from the parameter box...
        // or by iterating through the parameters in "paramBox" and pushing those
        // values in.
        // We have to consider array cases -- perhaps it easier to go through the
        // parameters in the parameter box
        for (const auto&i:paramBox) {
            auto name = i.Name();
            auto arrayBracket = std::find(name.begin(), name.end(), '[');
            if (arrayBracket == name.end()) {
                accessors.TryOpaqueSet(
                    obj,
                    Hash64(name.begin(), name.end()), i.RawValue(), 
                    i.Type(), false);
            } else {
                auto arrayIndex = XlAtoUI32((const char*)(arrayBracket+1));
                accessors.TryOpaqueSet(
                    obj, Hash64(name.begin(), arrayBracket), arrayIndex, i.RawValue(), 
                    i.Type(), false);
            }
        }
    }

}
