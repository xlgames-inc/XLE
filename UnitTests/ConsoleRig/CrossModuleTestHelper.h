// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include <string>
#include <memory>

namespace UnitTests
{
    class SingletonSharedFromMainModule1
    {
    public:
        std::string _identifyingString;

        static signed s_aliveCount;
        SingletonSharedFromMainModule1() { ++s_aliveCount; }
        ~SingletonSharedFromMainModule1() { --s_aliveCount; }
        SingletonSharedFromMainModule1(const SingletonSharedFromMainModule1&) = delete;

        signed _attachedModuleCount = 0;
        void AttachCurrentModule()
        {
            ++_attachedModuleCount;
        }
        void DetachCurrentModule()
        {
            --_attachedModuleCount;
        }
    };

    class SingletonSharedFromMainModule2
    {
    public:
        std::string _identifyingString;

        static signed s_aliveCount;
        SingletonSharedFromMainModule2() { ++s_aliveCount; }
        ~SingletonSharedFromMainModule2() { --s_aliveCount; }
        SingletonSharedFromMainModule2(const SingletonSharedFromMainModule1&) = delete;

        signed _attachedModuleCount = 0;
        void AttachCurrentModule()
        {
            ++_attachedModuleCount;
        }
        void DetachCurrentModule()
        {
            --_attachedModuleCount;
        }
    };

    class SingletonSharedFromAttachedModule
    {
    public:
    };
}
