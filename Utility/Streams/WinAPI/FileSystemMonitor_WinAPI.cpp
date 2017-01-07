// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../PathUtils.h"
#include "../../../Core/Prefix.h"
#include "../FileSystemMonitor.h"
#include "../../../Core/Types.h"
#include "../../../Core/WinAPI/IncludeWindows.h"
#include "../../Threading/Mutex.h"
#include "../../Threading/ThreadObject.h"
#include "../../Threading/LockFree.h"
#include "../../MemoryUtils.h"
#include "../../UTFUtils.h"
#include "../../IteratorUtils.h"
#include "../../Conversion.h"
#include <vector>
#include <memory>
#include <cctype>

// Character set note -- we're using utf16 characters in this file. We're going to
// assume that Windows is working with utf16 as well (as opposed to ucs2). This windows documentation
// suggest that it is expecting utf16 behaviour... But maybe there seems to be a lack of clarity as
// to how complete that support is.

namespace Utility
{

    class MonitoredDirectory
    {
    public:
        MonitoredDirectory(const std::basic_string<utf16>& directoryName);
        ~MonitoredDirectory();

        static uint64   HashFilename(StringSection<utf16> filename);
		static uint64   HashFilename(StringSection<utf8> filename);

        void            AttachCallback(uint64 filenameHash, std::shared_ptr<OnChangeCallback> callback);
        void            OnTriggered();

        void                BeginMonitoring();
        XlHandle            GetEventHandle() { return _overlapped.hEvent; }
        const OVERLAPPED*   GetOverlappedPtr() const { return &_overlapped; }
        unsigned            GetCreationOrderId() const { return _monitoringUpdateId; }

        void OnChange(StringSection<utf16> filename);
    private:
        std::vector<std::pair<uint64, std::weak_ptr<OnChangeCallback>>>  _callbacks;
        Threading::Mutex	_callbacksLock;
        XlHandle			_directoryHandle;
        uint8				_resultBuffer[1024];
        DWORD				_bytesReturned;
        OVERLAPPED			_overlapped;
		std::basic_string<utf16>	_directoryName;
        unsigned			_monitoringUpdateId;

        static void CALLBACK CompletionRoutine(
            DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
            LPOVERLAPPED lpOverlapped);
    };

    static Utility::Threading::Mutex MonitoredDirectoriesLock;
    static std::vector<std::pair<uint64, std::unique_ptr<MonitoredDirectory>>>  MonitoredDirectories;
    static unsigned CreationOrderId_Foreground = 0;
    static unsigned CreationOrderId_Background = 0;

    static XlHandle                                     RestartMonitoringEvent = INVALID_HANDLE_VALUE;
    static std::unique_ptr<Utility::Threading::Thread>  MonitoringThread;
    static Utility::Threading::Mutex                    MonitoringThreadLock;
    static bool                                         MonitoringQuit = false;

    MonitoredDirectory::MonitoredDirectory(const std::basic_string<utf16>& directoryName)
    : _directoryName(directoryName)
	{
        _overlapped.Internal = _overlapped.InternalHigh = 0;
        _overlapped.Offset = _overlapped.OffsetHigh = 0;
        _overlapped.hEvent = INVALID_HANDLE_VALUE; // XlCreateEvent(false);
        _directoryHandle = INVALID_HANDLE_VALUE;
        _bytesReturned = 0;
        _monitoringUpdateId = CreationOrderId_Foreground;
        XlSetMemory(_resultBuffer, dimof(_resultBuffer), 0);
    }

    MonitoredDirectory::~MonitoredDirectory()
    {
        if (_directoryHandle != INVALID_HANDLE_VALUE) {
            CancelIoEx(_directoryHandle, &_overlapped);
            CloseHandle(_directoryHandle);
        }
        CloseHandle(_overlapped.hEvent);
    }
	
	uint64 MonitoredDirectory::HashFilename(StringSection<utf16> filename)
    {
		return Utility::HashFilename(filename);
    }

	uint64 MonitoredDirectory::HashFilename(StringSection<utf8> filename)
	{
		return Utility::HashFilename(filename);
	}

    void MonitoredDirectory::AttachCallback(
        uint64 filenameHash, 
        std::shared_ptr<OnChangeCallback> callback)
    {
        ScopedLock(_callbacksLock);
        _callbacks.insert(
            LowerBound(_callbacks, filenameHash),
            std::make_pair(filenameHash, std::move(callback)));
    }

    void            MonitoredDirectory::OnTriggered()
    {
        FILE_NOTIFY_INFORMATION* notifyInformation = 
            (FILE_NOTIFY_INFORMATION*)_resultBuffer;
        for (;;) {

                //  Most editors just change the last write date when a file changes
                //  But some (like Visual Studio) will actually rename and replace the
                //  file (for error safety). To catch these cases, we need to look for
                //  file renames (also creation/deletion events would be useful)
            if (    notifyInformation->Action == FILE_ACTION_MODIFIED
                ||  notifyInformation->Action == FILE_ACTION_ADDED
                ||  notifyInformation->Action == FILE_ACTION_REMOVED
                ||  notifyInformation->Action == FILE_ACTION_RENAMED_OLD_NAME
                ||  notifyInformation->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                OnChange(StringSection<utf16>((const utf16*)notifyInformation->FileName, (const utf16*)PtrAdd(notifyInformation->FileName, notifyInformation->FileNameLength)));
            }

            if (!notifyInformation->NextEntryOffset) {
                break;
            }
            notifyInformation = PtrAdd(notifyInformation, notifyInformation->NextEntryOffset);
        }

            //      Restart searching
        _bytesReturned = 0;
        XlSetMemory(_resultBuffer, dimof(_resultBuffer), 0);
        BeginMonitoring();
    }

    void MonitoredDirectory::OnChange(StringSection<utf16> filename)
    {
        auto hash = MonitoredDirectory::HashFilename(filename);
        ScopedLock(_callbacksLock);
        #if 0 //(STL_ACTIVE == STL_MSVC) && (_ITERATOR_DEBUG_LEVEL >= 2)
            auto range = std::_Equal_range(
                _callbacks.begin(), _callbacks.end(), hash, 
                CompareFirst<uint64, std::weak_ptr<OnChangeCallback>>(),
                _Dist_type(_callbacks.begin()));
        #else
            auto range = std::equal_range(
                _callbacks.begin(), _callbacks.end(), 
                hash, CompareFirst<uint64, std::weak_ptr<OnChangeCallback>>());
        #endif

        bool foundExpired = false;
        for (auto i2=range.first; i2!=range.second; ++i2) {
                // todo -- what happens if OnChange() results in a change to _callbacks?
            auto l = i2->second.lock();
            if (l) l->OnChange();
            else foundExpired = true;
        }

        if (foundExpired) {
                // Remove any pointers that have expired
                // (note that we only check matching pointers. Non-matching pointers
                // that have expired are untouched)
            _callbacks.erase(
                std::remove_if(range.first, range.second, 
                    [](std::pair<uint64, std::weak_ptr<OnChangeCallback>>& i)
                    { return i.second.expired(); }),
                range.second);
        }
    }

    void CALLBACK MonitoredDirectory::CompletionRoutine(
        DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
        LPOVERLAPPED lpOverlapped )
    {
            //  when a completion routine triggers, we need to look for the directory
            //  monitor that matches the overlapped ptr
        if (dwErrorCode == ERROR_SUCCESS) {
            ScopedLock(MonitoredDirectoriesLock);
            for (auto i=MonitoredDirectories.begin(); i!=MonitoredDirectories.end(); ++i) {
                if (i->second->GetOverlappedPtr() == lpOverlapped) {
                    i->second->OnTriggered();
                }
            }
        }
    }

    void MonitoredDirectory::BeginMonitoring()
    {
        if (_directoryHandle == INVALID_HANDLE_VALUE) {
            _directoryHandle = CreateFileW(
                (LPCWSTR)_directoryName.c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, nullptr);
        }

        auto hresult = ReadDirectoryChangesW(
            _directoryHandle, _resultBuffer, sizeof(_resultBuffer),
            FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            /*&_bytesReturned*/nullptr, &_overlapped, &CompletionRoutine);
        /*assert(hresult);*/ (void)hresult;
    }

    unsigned int xl_thread_call MonitoringEntryPoint(void*)
    {
        while (!MonitoringQuit) {
            {
                ScopedLock(MonitoredDirectoriesLock);
                auto newId = CreationOrderId_Background;
                for (auto i=MonitoredDirectories.begin(); i!=MonitoredDirectories.end(); ++i) {
                    auto updateId = i->second->GetCreationOrderId();
                    if (updateId > CreationOrderId_Background) {
                        i->second->BeginMonitoring();
                        newId = std::max(newId, updateId);
                    }
                }
                CreationOrderId_Background = newId;
            }

            XlWaitForMultipleSyncObjects(
                1, &RestartMonitoringEvent, 
                false, XL_INFINITE, true);
        }

        return 0;
    }

    static void RestartMonitoring()
    {
        ScopedLock(MonitoringThreadLock);
        if (!MonitoringThread) {
            RestartMonitoringEvent = XlCreateEvent(false);
            MonitoringThread = std::make_unique<Utility::Threading::Thread>(MonitoringEntryPoint, nullptr);
        }
        XlSetEvent(RestartMonitoringEvent);
    }

    void TerminateFileSystemMonitoring()
    {
        {
            ScopedLock(MonitoringThreadLock);
            if (MonitoringThread) {
                MonitoringQuit = true;
                XlSetEvent(RestartMonitoringEvent);
                MonitoringThread->join();
                MonitoringThread.reset();
            }
        }
        {
            ScopedLock(MonitoredDirectoriesLock);
            MonitoredDirectories.clear();
        }
    }

	/*static void CheckExists(const utf16* dirName)
	{
		#if defined(_DEBUG)
            {
                    // verify that it exists
                auto handle = CreateFileW(
                    (LPCWSTR)dirName, FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, nullptr);
                assert(handle != INVALID_HANDLE_VALUE);
                CloseHandle(handle);
            }
        #endif
	}*/

    void AttachFileSystemMonitor(
        StringSection<utf16> directoryName,
        StringSection<utf16> filename,
        std::shared_ptr<OnChangeCallback> callback)
    {
        ScopedLock(MonitoredDirectoriesLock);
        if (directoryName.IsEmpty())
            directoryName = StringSection<utf16>(u"./");

        auto hash = MonitoredDirectory::HashFilename(directoryName);
        auto i = std::lower_bound(
            MonitoredDirectories.cbegin(), MonitoredDirectories.cend(), 
            hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
        if (i != MonitoredDirectories.cend() && i->first == hash) {
            i->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));
            return;
        }

            // we must have a null terminated string -- so use a temp buffer
        auto dirNameCopy = directoryName.AsString();
		// CheckExists(dirNameCopy.c_str());

        ++CreationOrderId_Foreground;
        auto i2 = MonitoredDirectories.insert(
            i, std::make_pair(hash, std::make_unique<MonitoredDirectory>(dirNameCopy)));
        i2->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));

            //  we need to trigger the background thread so that it begins the the ReadDirectoryChangesW operation
            //  (that operation must begin and be handled in the same thread when using completion routines)
        RestartMonitoring();
    }

	void AttachFileSystemMonitor(
		StringSection<utf8> directoryName,
		StringSection<utf8> filename,
		std::shared_ptr<OnChangeCallback> callback)
	{
		ScopedLock(MonitoredDirectoriesLock);
		if (directoryName.IsEmpty())
			directoryName = StringSection<utf8>(u("./"));

		auto hash = MonitoredDirectory::HashFilename(directoryName);
		auto i = std::lower_bound(
			MonitoredDirectories.cbegin(), MonitoredDirectories.cend(),
			hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
		if (i != MonitoredDirectories.cend() && i->first == hash) {
			i->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));
			return;
		}

		// we must have a null terminated string -- so use a temp buffer
		auto dirNameCopy = Conversion::Convert<std::basic_string<utf16>>(directoryName.AsString());
		// CheckExists(dirNameCopy.c_str());

		++CreationOrderId_Foreground;
		auto i2 = MonitoredDirectories.insert(
			i, std::make_pair(hash, std::make_unique<MonitoredDirectory>(dirNameCopy)));
		i2->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));

		//  we need to trigger the background thread so that it begins the the ReadDirectoryChangesW operation
		//  (that operation must begin and be handled in the same thread when using completion routines)
		RestartMonitoring();
	}

    void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename)
    {
        ScopedLock(MonitoredDirectoriesLock);
        auto hash = MonitoredDirectory::HashFilename(directoryName);
        auto i = std::lower_bound(
            MonitoredDirectories.cbegin(), MonitoredDirectories.cend(), 
            hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
        if (i != MonitoredDirectories.cend() && i->first == hash) {
            i->second->OnChange(filename);
        }
    }

	void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename)
	{
		ScopedLock(MonitoredDirectoriesLock);
		auto hash = MonitoredDirectory::HashFilename(directoryName);
		auto i = std::lower_bound(
			MonitoredDirectories.cbegin(), MonitoredDirectories.cend(),
			hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
		if (i != MonitoredDirectories.cend() && i->first == hash) {
			auto u16name = Conversion::Convert<std::basic_string<utf16>>(filename);
			i->second->OnChange(MakeStringSection(u16name));
		}
	}


    OnChangeCallback::~OnChangeCallback() {}
    
}

