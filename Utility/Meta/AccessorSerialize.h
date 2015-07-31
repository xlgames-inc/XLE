// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace Utility
{
    class OutputStreamFormatter;
    class ClassAccessors;
}

template<typename Type> const Utility::ClassAccessors& GetAccessors();

namespace Utility
{
    template<typename Formatter>
        void AccessorDeserialize(
            Formatter& formatter,
            void* obj, const ClassAccessors& props);

    void AccessorSerialize(
        OutputStreamFormatter& formatter,
        const void* obj, const ClassAccessors& props);

    template<typename Formatter, typename Type>
        void AccessorDeserialize(
            Formatter& formatter,
            Type& obj)
        {
            const auto& props = GetAccessors<Type>();
            AccessorDeserialize(formatter, &obj, props);
        }


    template<typename Type>
        void AccessorSerialize(
            OutputStreamFormatter& formatter,
            const Type& obj)
        {
            const auto& props = GetAccessors<Type>();
            AccessorSerialize(formatter, &obj, props);
        }

}

using namespace Utility;


