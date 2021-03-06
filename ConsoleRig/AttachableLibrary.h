// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace ConsoleRig
{
    class LibVersionDesc
    {
    public:
        const char* _versionString;
        const char* _buildDateString;
    };

    class AttachableLibrary
    {
    public:
        bool TryAttach();
        void Detach();

        bool TryGetVersion(LibVersionDesc&);

        template<typename FnSignature>
            FnSignature GetFunction(const char name[]);

        typedef char CharType;

        AttachableLibrary(const CharType filename[]);
        ~AttachableLibrary();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        void* GetFunctionAddress(const char name[]);
    };

    template<typename FnSignature>
        FnSignature AttachableLibrary::GetFunction(const char name[])
    {
            // We can't do any type checking on
            // this pointer -- so we have just to cast and return it.
            // We could do type checking if we implemented a wrapper
            // function around the real function that was exposed, I
            // guess... Then the type checking could be performed
            // in the dll module, not this module.
        void* adr = GetFunctionAddress(name);
        FnSignature result;
        *(void**)&result = adr;
        return result;
    }
}
