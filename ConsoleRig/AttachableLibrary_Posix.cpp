// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AttachableLibrary.h"
#include "GlobalServices.h"
#include "AttachablePtr.h"
#include "../OSServices/Log.h"
#include <dlfcn.h>
#include <assert.h>

namespace ConsoleRig
{
    class AttachableLibrary::Pimpl
    {
    public:
        signed _attachCount = 0;
        void* _library = nullptr;
        std::string _filename;
        LibVersionDesc _dllVersion = LibVersionDesc { "Unknown", "Unknown" };
        bool _versionInfoValid = false;
    };

    bool AttachableLibrary::TryAttach(std::string& errorMsg)
    {
        if (!_pimpl->_attachCount) {
            auto* prevError = dlerror();
            if (prevError)
                Log(Error) << "Pre-existing error returned from dlerror: " << prevError << std::endl;

            assert(_pimpl->_library == nullptr);
            _pimpl->_library = dlopen(_pimpl->_filename.c_str(), RTLD_NOW | RTLD_LOCAL);

            if (!_pimpl->_library) {
                errorMsg = dlerror();
                return false;
            }

            auto attachFn = (void (*)(ConsoleRig::CrossModule&))dlsym(_pimpl->_library, "AttachLibrary");
            auto getVersionInfoFn = (LibVersionDesc (*)())dlsym(_pimpl->_library, "GetVersionInformation");
            if (attachFn) {
				(*attachFn)(ConsoleRig::CrossModule::GetInstance());
			}

            if (getVersionInfoFn) {
                _pimpl->_dllVersion = (*getVersionInfoFn)();
                _pimpl->_versionInfoValid = true;
            }
        }

        ++_pimpl->_attachCount;
        return true;
    }

    void AttachableLibrary::Detach()
    {
        assert(_pimpl->_attachCount > 0);
        --_pimpl->_attachCount;

        if (_pimpl->_attachCount==0) {
            assert(_pimpl->_library != nullptr);

                // If there is a "DetachLibrary" function, we should
                // call it now.
			auto detachFn = (void (*)())dlsym(_pimpl->_library, "DetachLibrary");
			if (detachFn) {
				(*detachFn)();
			}

			dlclose(_pimpl->_library);
            _pimpl->_library = nullptr;
        }
    }

    bool AttachableLibrary::TryGetVersion(LibVersionDesc& output)
    {
        output = _pimpl->_dllVersion;
        return _pimpl->_versionInfoValid;
    }

    void* AttachableLibrary::GetFunctionAddress(const char name[])
    {
        if (_pimpl->_attachCount <= 0) return nullptr;
        return dlsym(_pimpl->_library, name);
    }

    AttachableLibrary::AttachableLibrary(StringSection<CharType> filename)
    {
        _pimpl = std::make_unique<Pimpl>();

        // We want to avoid invoking the library search mechanism -- which can be done
        // by ensuring that there's a slash in the filename
        _pimpl->_filename = "./" + filename.AsString();
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

