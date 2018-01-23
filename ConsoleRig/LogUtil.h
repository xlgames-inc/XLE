#pragma once

#include "Log.h"
#include "../Foreign/fmt/format.h"
#include "../Foreign/fmt/ostream.h"

#if defined(__OBJC__)
    #include "Externals/Misc/OCPtr.h"

    inline std::ostream& operator<<(std::ostream& stream, NSObject* objcObj)
    {
        if (!objcObj) return stream << "<<nil>>";
        return stream << objcObj.description.UTF8String;
    }

    template<typename T>
        inline std::ostream& operator<<(std::ostream& stream, const TBC::OCPtr<T>& objcObj)
    {
        if (!objcObj) return stream << "<<nil>>";
        return stream << objcObj.get().description.UTF8String;
    }

    struct ObjC { NSObject* _obj; };

    inline std::ostream& operator<<(std::ostream& stream, ObjC objcObj)
    {
        if (!objcObj._obj) return stream << "<<nil>>";
        return stream << objcObj._obj.description.UTF8String;
    }
#endif

using namespace fmt::literals;

#if defined(CORESERVICES_ENABLELOG)
    inline void LogLine(ConsoleRig::MessageTarget<>& target, const char *format, fmt::ArgList args)
    {
        Log(target) << fmt::format(format, args) << std::endl;
    }

    FMT_VARIADIC(void, LogLine, ConsoleRig::MessageTarget<>&, const char *);
#else
    #define LogLine(...)
#endif
