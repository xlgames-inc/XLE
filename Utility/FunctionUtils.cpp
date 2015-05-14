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

    bool VariantFunctions::Remove(Id id)
    {
        // if the given function exists, we need to remove it from our buffer
        // we're going to do a reallocation of the buffer for every call here.
        // We can't implement a normal form of "erase", because the objects are
        // all different sizes... 
        // This means, when we attempt to call the move constructor while moving
        // objects to fill the new space, we can get situations where there is an
        // overlap between the src and the destination of the move operation.
        // Consider what happens if we have a small object in the buffer, followed
        // by a larger object. If we erase the small object, we can't rearrange the
        // buffer safely.
        // So, let's just reallocate each time...

        auto i = LowerBound(_fns, id);
        if (i == _fns.end() || i->first != id)
            return false;

        if (_fns.size() == 1) {
                // if we're erasing our last one, we can just destroy it, and
                // erase our buffers completely
            assert(i->second._offset == 0);
            i->second._destructor(AsPointer(_buffer.begin()));
            std::vector<uint8_t>().swap(_buffer);
            std::vector<std::pair<Id, StoredFunction>>().swap(_fns);
            return true;
        }

        std::vector<uint8_t> newBuffer;
        auto newSize = _buffer.size() - i->second._size;
        newBuffer.reserve(newSize * 2);
        newBuffer.insert(newBuffer.begin(), newSize, uint8_t(0xcd));

        ptrdiff_t offsetAdjust =-ptrdiff_t(i->second._size);
        size_t displaceStart = i->second._offset;
        for (auto q=_fns.begin(); q!=_fns.end(); ++q) {
            auto& f = q->second;
            if (q == i) {
                    // this is the one to destroy
                f._destructor(PtrAdd(AsPointer(_buffer.begin()), f._offset));
            } else {
                auto newOffset = f._offset;
                if (newOffset > displaceStart) {
                    assert((ptrdiff_t(newOffset) + offsetAdjust) >= ptrdiff_t(displaceStart));
                    newOffset += offsetAdjust;
                }

                assert((newOffset + f._size) <= newBuffer.size());
                assert((f._offset + f._size) <= _buffer.size());

                f._moveConstructor(
                    PtrAdd(AsPointer(newBuffer.begin()), newOffset), 
                    PtrAdd(AsPointer(_buffer.begin()), f._offset));
                f._destructor(PtrAdd(AsPointer(_buffer.begin()), f._offset));
                f._offset = newOffset;
            }
        }

        _buffer = std::move(newBuffer);
        _fns.erase(i);

        return true;
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

