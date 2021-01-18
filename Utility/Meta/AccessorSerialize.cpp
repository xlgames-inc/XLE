// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AccessorSerialize.h"
#include "ClassAccessors.h"
#include "ClassAccessorsImpl.h"

#include "../../OSServices/Log.h"
#include "../Streams/StreamFormatter.h"
#include "../StringFormat.h"
#include "../ParameterBox.h"
#include "../MemoryUtils.h"
#include "../Conversion.h"

#include <iostream>

// #define SUPPORT_POLYMORPHIC_EXTENSIONS

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
                    
                    if (!props.SetFromString( obj, name, value)) {
                        std::cout << "Failure while assigning property during deserialization -- " <<
                            Conversion::Convert<std::string>(std::basic_string<CharType>(name._start, name._end)) << std::endl;
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
#if SUPPORT_POLYMORPHIC_EXTENSIONS
                    auto* legacyExtensions = dynamic_cast<const Legacy::ClassAccessorsWithChildLists*>(&props)
                    if (legacyExtensions) {
                        typename Formatter::InteriorSection eleName;
                        if (!formatter.TryBeginElement(eleName))
                            Throw(FormatException("Error in begin element", formatter.GetLocation()));

                        auto created = legacyExtensions->TryCreateChild(obj, Hash64(eleName._start, eleName._end));
                        if (created.first) {
                            AccessorDeserialize(formatter, created.first, *created.second);
                        } else {
                            std::cout << "Couldn't find a match for element name during deserialization -- " <<
                                Conversion::Convert<std::string>(std::basic_string<CharType>(eleName._start, eleName._end)) << std::endl;
                            formatter.SkipElement();
                        }

                        if (!formatter.TryEndElement())
                            Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    } else 
#endif
                    {
                        Throw(FormatException("Children elements not supported for this type", formatter.GetLocation()));
                    }

                    break;
                }
            }
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void AccessorSerialize(
        OutputStreamFormatter& formatter,
        const void* obj, const ClassAccessors& accessors)
    {
        for (const auto&p:accessors.GetProperties()) {
            auto str = accessors.GetAsString(obj, p.first);
            if (!str.has_value()) continue;

            auto v = str.value();
            formatter.WriteAttribute(
                MakeStringSection(p.second._name),
                MakeStringSection(str.value()));
        }

#if SUPPORT_POLYMORPHIC_EXTENSIONS
        auto* legacyExtensions = dynamic_cast<const Legacy::ClassAccessorsWithChildLists*>(&accessors)
        if (legacyExtensions) {
            for (size_t i=0; i<legacyExtensions->GetChildListCount(); ++i) {
                const auto& childList = legacyExtensions->GetChildListByIndex(i);
                auto count = childList._getCount(obj);
                for (size_t e=0; e<count; ++e) {
                    const auto* child = childList._getByIndex(obj, e);
                    auto eleId = formatter.BeginElement(childList._name);
                    AccessorSerialize(formatter, child, *childList._childProps);
                    formatter.EndElement(eleId);
                }
            }
        }
#endif
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
            accessors.Set(
                obj,
                name, i.RawValue(), 
                i.Type());
        }
    }

}
