// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include "Stream.h"

namespace Utility
{

    // --------------------------------------------------------------------------
    // Memory Input
    // --------------------------------------------------------------------------

    class XL_UTILITY_API MemoryInputStream : public InputStream 
    {
    public:
        const uint8* _ptr;
        const uint8* _begin;
        const uint8* _end;

        MemoryInputStream(const void* s, int len);
        virtual int Read(void* p, int len);
        virtual bool Seek(StreamSeekType type, int64 offset);
    };

    // --------------------------------------------------------------------------
    // Memory Output
    // --------------------------------------------------------------------------

    class XL_UTILITY_API MemoryOutputStream : public OutputStream 
    {
    public:
        uint8* _ptr;
        const uint8* _begin;
        uint8* _end;

        MemoryOutputStream(void* s, int len);

        uint8* Cursor() { return _ptr; }
        const uint8* Begin() const { return _begin; }
        uint8* End() const { return _end; }


        virtual Type GetType() const { return OSTRM_MEM; }
        virtual int64 Tell();
        virtual int64 Write(const void* p, int len);

        virtual void WriteChar(utf8 ch);
        virtual void WriteChar(ucs2 ch);
        virtual void WriteChar(ucs4 ch);
        virtual void WriteString(const utf8* s);
        virtual void WriteString(const ucs2* s);
        virtual void WriteString(const ucs4* s);

        virtual void Flush() {}

        // type := naturally SEEK_CUR
        bool Seek(StreamSeekType /*type*/, int64 offset)
        {
            uint8* p = _ptr + offset;
            if (p < _begin || p >= _end)
                return false;
            _ptr = p;
            return true;
        }

    };

    // --------------------------------------------------------------------------
    // String Output
    // --------------------------------------------------------------------------

    template<typename T>
        class StringOutputStream : public MemoryOutputStream 
    {
    public:
        StringOutputStream(T* s, int len);
        ~StringOutputStream();

        virtual Type GetType() const        { return OSTRM_STR; }
        inline operator bool() const        { return (bool)(_ptr < _end); }
    };

    template<typename T> StringOutputStream<T>::StringOutputStream(T* s, int len) 
    : MemoryOutputStream(s, sizeof(T) * len)
    {
    }

    template<typename T> StringOutputStream<T>::~StringOutputStream()
    {
            // (string terminator)
        if (_ptr + sizeof(T) <= _end) {
            *(T*)_ptr = '\0';
        } else {
            ((T*)_end)[-1] = '\0';
        }
    }

}

using namespace Utility;
