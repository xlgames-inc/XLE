// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ClassAccessors.h"
#include "../ParameterBox.h"

namespace Utility
{
    bool ClassAccessors::TryOpaqueSet(
        void* dst, uint64 id,
        IteratorRange<const void*> src, ImpliedTyping::TypeDesc srcType,
        bool stringForm) const
    {
		assert(src.size() == srcType.GetSize());
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id) {
            if (i->second._castFrom)
                return i->second._castFrom(dst, src.begin(), srcType, stringForm);

            if (i->second._castFromArray) {
                    // If there is an array form, then we can try to
                    // set all of the members of the array at the same time
                    // First, we'll use the implied typing system to break down
                    // our input into array components.. Then we'll set each
                    // element individually.
                char buffer[256];
                if (stringForm) {
                    auto parsedType = ImpliedTyping::ParseFullMatch(
                        MakeStringSection((const char*)src.begin(), (const char*)src.end()),
                        buffer, sizeof(buffer));
                    if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                    srcType = parsedType;
					src = {buffer, PtrAdd(buffer, srcType.GetSize())};
                }

                bool result = false;
                auto elementDesc = ImpliedTyping::TypeDesc{srcType._type};
                auto elementSize = ImpliedTyping::TypeDesc{srcType._type}.GetSize();
                for (unsigned c=0; c<srcType._arrayCount; ++c) {
                    auto* e = PtrAdd(src.begin(), c*elementSize);
					assert(PtrAdd(e, elementDesc.GetSize()) <= src.end());
                    result |= i->second._castFromArray(dst, c, e, elementDesc, false);
                }
                return result;
            }
        }

        return false;
    }

    bool ClassAccessors::TryOpaqueSet(
        void* dst,
        uint64 id, size_t arrayIndex,
        IteratorRange<const void*> src,
        ImpliedTyping::TypeDesc srcType,
        bool stringForm) const
    {
		assert(src.size() == srcType.GetSize());
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id)
            return i->second._castFromArray(dst, arrayIndex, src.begin(), srcType, stringForm);
        return false;
    }

    bool ClassAccessors::TryOpaqueGet(
        void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType,
        const void* src, uint64 id,
        bool stringForm) const
    {
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id) {
            if (i->second._castTo)
                return i->second._castTo(src, dst, dstSize, dstType, stringForm);

            // note -- array form not supported
        }

        return false;
    }

    bool ClassAccessors::TryGetNaturalType(ImpliedTyping::TypeDesc& result, uint64 id) const
    {
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id) {
            result = i->second._naturalType;
            return true;
        }
		return false;
	}

    std::pair<void*, const ClassAccessors*> ClassAccessors::TryCreateChild(
        void* dst, uint64 childListId) const
    {
        auto i = LowerBound(_childLists, childListId);
        if (i!=_childLists.end() && i->first == childListId) {
            void* created = i->second._createFn(dst);
            return std::make_pair(created, i->second._childProps);
        }
        return std::make_pair(nullptr, nullptr);
    }

    auto ClassAccessors::PropertyForId(uint64 id) -> Property&
    {
        auto i = LowerBound(_properties, id);
        if (i==_properties.end() || i->first != id)
            i=_properties.insert(i, std::make_pair(id, Property()));
        return i->second;
    }

    ClassAccessors::ClassAccessors(size_t associatedType) 
        : _associatedType(associatedType) {}
    ClassAccessors::~ClassAccessors() {}
}

