// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AttachableLibrary.h"

namespace ConsoleRig
{
    class AttachableLibrary::Pimpl
    {
    public:
    };

    bool AttachableLibrary::TryAttach(std::string& errorMsg)
    {
        return false;
    }

    void AttachableLibrary::Detach()
    {
    }

    bool AttachableLibrary::TryGetVersion(LibVersionDesc& output)
    {
        return false;
    }

    void* AttachableLibrary::GetFunctionAddress(const char name[])
    {
        return nullptr;
    }

    AttachableLibrary::AttachableLibrary(StringSection<CharType> filename)
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    AttachableLibrary::~AttachableLibrary()
    {
    }

	AttachableLibrary::AttachableLibrary(AttachableLibrary&& moveFrom) never_throws
	: _pimpl(std::move(moveFrom._pimpl))
	{}

	AttachableLibrary& AttachableLibrary::operator=(AttachableLibrary&& moveFrom) never_throws
	{
		_pimpl = std::move(moveFrom._pimpl);
		return *this;
	}
}

