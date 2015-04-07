// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
#include <vector>
#include <memory>

namespace Utility
{

    class MonitoredDirectory
    {
    public:
        MonitoredDirectory(const char directoryName[]);
        ~MonitoredDirectory();

        static uint64   HashFilename(const char filename[]);
        void            AttachCallback(uint64 filenameHash, std::shared_ptr<OnChangeCallback> callback);
        void            OnTriggered();

        void                BeginMonitoring();
        XlHandle            GetEventHandle() { return _overlapped.hEvent; }
        const OVERLAPPED*   GetOverlappedPtr() const { return &_overlapped; }
        unsigned            GetCreationOrderId() const { return _monitoringUpdateId; }

        void OnChange(const char filename[]);
    private:
        std::vector<std::pair<uint64, std::shared_ptr<OnChangeCallback>>>  _callbacks;
        Threading::Mutex  _callbacksLock;
        XlHandle        _directoryHandle;
        uint8           _resultBuffer[1024];
        DWORD           _bytesReturned;
        OVERLAPPED      _overlapped;
        std::string     _directoryName;
        unsigned        _monitoringUpdateId;

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

    MonitoredDirectory::MonitoredDirectory(const char directoryName[])
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

    uint64          MonitoredDirectory::HashFilename(const char filename[])
    {
        char buffer[MaxPath];
        char *b = buffer;
        while (*filename !='\0' && b!=&buffer[MaxPath]) {
            *b = (char)tolower(*filename); ++filename; ++b;
        }
        return Hash64(buffer, b);
    }

    void            MonitoredDirectory::AttachCallback(uint64 filenameHash, std::shared_ptr<OnChangeCallback> callback)
    {
        ScopedLock(_callbacksLock);
        _callbacks.insert(
            std::lower_bound(   _callbacks.cbegin(), _callbacks.cend(), 
                                filenameHash, CompareFirst<uint64, std::shared_ptr<OnChangeCallback>>()),
            std::make_pair(filenameHash, callback));
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

                char buffer[MaxPath];
                buffer[0] = '\0';
                auto destSize = ucs2_2_utf8(
                    (const ucs2*)notifyInformation->FileName, notifyInformation->FileNameLength / sizeof(ucs2),
                    (utf8*)buffer, dimof(buffer));
                buffer[std::min(size_t(destSize), dimof(buffer)-1)] = '\0';

                OnChange((const char*)buffer);
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

    void MonitoredDirectory::OnChange(const char filename[])
    {
        auto hash = MonitoredDirectory::HashFilename(filename);
        ScopedLock(_callbacksLock);
        auto i = std::equal_range(
            _callbacks.cbegin(), _callbacks.cend(), 
            hash, CompareFirst<uint64, std::shared_ptr<OnChangeCallback>>());
        for (auto i2=i.first; i2!=i.second; ++i2) {
                // todo -- what happens if OnChange() results in a change to _callbacks?
            i2->second->OnChange();
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
            _directoryHandle = CreateFile(
                _directoryName.c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, nullptr);
        }

        auto hresult = ReadDirectoryChangesW(
            _directoryHandle, _resultBuffer, sizeof(_resultBuffer),
            FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            /*&_bytesReturned*/nullptr, &_overlapped, &CompletionRoutine);
        assert(hresult); (void)hresult;
    }

    unsigned int xl_thread_call MonitoringEntryPoint(void*)
    {
        while (!MonitoringQuit) {
            std::vector<XlHandle> events;
            events.push_back(RestartMonitoringEvent);

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
                uint32(events.size()), (XlHandle*)AsPointer(events.cbegin()), 
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

    void AttachFileSystemMonitor(   const char directoryName[], const char filename[], 
                                    std::shared_ptr<OnChangeCallback> callback)
    {
        ScopedLock(MonitoredDirectoriesLock);
        assert(directoryName && directoryName[0]);

        auto hash = MonitoredDirectory::HashFilename(directoryName);
        auto i = std::lower_bound(
            MonitoredDirectories.cbegin(), MonitoredDirectories.cend(), 
            hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
        if (i != MonitoredDirectories.cend() && i->first == hash) {
            i->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));
            return;
        }

        #if defined(_DEBUG)
            {
                auto handle = CreateFile(
                    directoryName, FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, nullptr);
                assert(handle != INVALID_HANDLE_VALUE);
                CloseHandle(handle);
            }
        #endif

        ++CreationOrderId_Foreground;
        auto i2 = MonitoredDirectories.insert(i, std::make_pair(hash, std::make_unique<MonitoredDirectory>(directoryName)));
        i2->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));

            //  we need to trigger the background thread so that it begins the the ReadDirectoryChangesW operation
            //  (that operation must begin and be handled in the same thread when using completion routines)
        RestartMonitoring();
    }

    void    FakeFileChange(const char directoryName[], const char filename[])
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
    
}

