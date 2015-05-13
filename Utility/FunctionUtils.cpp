// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FunctionUtils.h"

namespace Utility
{
    void VariantFunctions::ExpandBuffer(size_t newCapacity)
    {
        if (newCapacity <= _buffer.capacity()) return;

            //  std::function<> objects have internal pointers,
            //  so we have to use the correct process of calling
            //  move operators and destructors when resizing the
            //  buffer. Since the actual std::vector<> isn't using
            //  the correct types, that means we'll have to write
            //  a custom resizing function here...
        std::vector<uint8_t> newBuffer;
        newBuffer.reserve(newCapacity);
        newBuffer.insert(newBuffer.begin(), _buffer.size(), uint8_t(0xcd));

            //  The following is not reserveable if an exception happens.
            //  If we get an exception from the move operation of the 
            //  std::function<> object, we'll end up in an invalid state.
            //  But does the move operation of std::function<> ever throw?
        for (auto i:_fns) {
            auto& f = i.second;
            f._moveConstructor(
                PtrAdd(AsPointer(newBuffer.begin()), f._offset), 
                PtrAdd(AsPointer(_buffer.begin()), f._offset));
            f._destructor(PtrAdd(AsPointer(_buffer.begin()), f._offset));
        }

        _buffer = std::move(newBuffer);
    }

    VariantFunctions::VariantFunctions() {}
    VariantFunctions::~VariantFunctions()
    {
        // Ok, here's the crazy part.. we want to call the destructors
        // of all of the types we've stored in here... But we don't know their
        // types! But we have a pointer to a function that will call their
        // destructor. So we just need to call that.
        for (auto i=_fns.begin(); i!=_fns.end(); ++i) {
            auto* obj = (void*)PtrAdd(AsPointer(_buffer.begin()), i->second._offset);
            (*i->second._destructor)(obj);
        }
    }
}

