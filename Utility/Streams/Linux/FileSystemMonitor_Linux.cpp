// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../PathUtils.h"
#include "../FileSystemMonitor.h"
#include "../../Threading/Mutex.h"
#include "../../Threading/LockFree.h"
#include "../../MemoryUtils.h"
#include "../../UTFUtils.h"
#include "../../IteratorUtils.h"
#include "../../Conversion.h"
#include <vector>
#include <memory>
#include <cctype>

#include <sys/inotify.h>
#include <poll.h>

// #include <iostream>

// #define ENABLE_FILESYSTEM_MONITORING

#if defined(ENABLE_FILESYSTEM_MONITORING)

namespace Utility
{
    class MonitoredDirectory
    {
    public:
        void            AttachCallback(uint64_t filenameHash, std::shared_ptr<OnChangeCallback> callback);
        void            OnChange(StringSection<> filename);
        int             wd() const { return _wd; }

        static uint64_t HashFilename(StringSection<utf16> filename);
        static uint64_t HashFilename(StringSection<utf8> filename);
        static uint64_t HashFilename(StringSection<> filename);

        MonitoredDirectory(const std::string& directoryName, int wd);
        ~MonitoredDirectory();
    private:
        std::vector<std::pair<uint64_t, std::weak_ptr<OnChangeCallback>>>  _callbacks;
        Threading::Mutex	_callbacksLock;

		std::string     _directoryName;
        unsigned        _monitoringUpdateId;
        int             _wd;
    };

    static Utility::Threading::Mutex MonitoredDirectoriesLock;
    static std::vector<std::pair<uint64_t, std::unique_ptr<MonitoredDirectory>>>  MonitoredDirectories;
    static unsigned CreationOrderId_Foreground = 0;

    static int inotify_fd = -1;

    MonitoredDirectory::MonitoredDirectory(const std::string& directoryName, int wd)
    : _directoryName(directoryName), _wd(wd)
	{
        _monitoringUpdateId = CreationOrderId_Foreground;
    }

    MonitoredDirectory::~MonitoredDirectory()
    {
    }

	uint64_t MonitoredDirectory::HashFilename(StringSection<utf16> filename)  { return Utility::HashFilename(filename); }
    uint64_t MonitoredDirectory::HashFilename(StringSection<utf8> filename) { return Utility::HashFilename(filename); }
    uint64_t MonitoredDirectory::HashFilename(StringSection<> filename) { return Utility::HashFilename(MakeStringSection((utf8*)filename.begin(), (utf8*)filename.end())); }

    void MonitoredDirectory::AttachCallback(
        uint64 filenameHash,
        std::shared_ptr<OnChangeCallback> callback)
    {
        ScopedLock(_callbacksLock);
        _callbacks.insert(
            LowerBound(_callbacks, filenameHash),
            std::make_pair(filenameHash, std::move(callback)));
    }

    void MonitoredDirectory::OnChange(StringSection<> filename)
    {
        // std::cout << "OnChange for filename: " << filename.AsString().c_str() << std::endl;

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

    static std::unique_ptr<std::thread> MonitoringThread;

    static void RestartMonitoring()
    {
        if (!MonitoringThread) {
            MonitoringThread = std::make_unique<std::thread>([]() {
                struct pollfd fds[1];
                fds[0].fd = inotify_fd;
                fds[0].events = POLLIN;
                fds[0].revents = 0;
                for (;;) {
                    auto poll_num = poll(fds, dimof(fds), -1);
                    if (poll_num == -1)
                        break;

                    if (poll_num > 0) {
                        bool cancelPolling = false;
                        if (fds[0].revents & POLLIN) {
                            char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
                            for (;;) {
                                /* Read some events. */
                                auto len = read(inotify_fd, buf, sizeof buf);
                                /*if (len == -1 && errno != EAGAIN) {
                                    cancelPolling = true;
                                    break;
                                }*/
                                if (len <= 0)
                                    break;

                                const struct inotify_event *event;
                                for (auto* ptr = buf; ptr < buf + len;
                                     ptr += sizeof(struct inotify_event) + event->len) {

                                    event = (const struct inotify_event *) ptr;

                                    ScopedLock(MonitoredDirectoriesLock);
                                    for (const auto&m:MonitoredDirectories)
                                        if (m.second->wd() == event->wd)
                                            m.second->OnChange(event->name);
                                }
                            }
                        }
                        if (cancelPolling)
                            break;
                    }
                }
            });
        }
    }

    void TerminateFileSystemMonitoring()
    {
        {
            ScopedLock(MonitoredDirectoriesLock);
            MonitoredDirectories.clear();
        }
    }

    void AttachFileSystemMonitor(
        StringSection<utf16> directoryName,
        StringSection<utf16> filename,
        std::shared_ptr<OnChangeCallback> callback)
    {
        assert(0);      // not implemented
    }

	void AttachFileSystemMonitor(
		StringSection<utf8> directoryName,
		StringSection<utf8> filename,
		std::shared_ptr<OnChangeCallback> callback)
	{
        if (inotify_fd == -1)
            inotify_fd = inotify_init();

        // std::cout << "Register (" << (char*)directoryName.AsString().c_str() << ", " << (char*)filename.AsString().c_str() << ")" << std::endl;

        {
            ScopedLock(MonitoredDirectoriesLock);
            if (directoryName.IsEmpty())
                directoryName = StringSection<utf8>("./");

            auto hash = MonitoredDirectory::HashFilename(directoryName);
            auto i = std::lower_bound(
                MonitoredDirectories.cbegin(), MonitoredDirectories.cend(),
                hash, CompareFirst<uint64, std::unique_ptr<MonitoredDirectory>>());
            if (i != MonitoredDirectories.cend() && i->first == hash) {
                i->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));
                return;
            }

            auto directoryNameCopy = Conversion::Convert<std::string>(directoryName.AsString());
            auto wd = inotify_add_watch(inotify_fd, directoryNameCopy.c_str(), IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);

            ++CreationOrderId_Foreground;
            auto i2 = MonitoredDirectories.insert(
                i, std::make_pair(hash, std::make_unique<MonitoredDirectory>(directoryNameCopy, wd)));
            i2->second->AttachCallback(MonitoredDirectory::HashFilename(filename), std::move(callback));
        }

		//  we need to trigger the background thread so that it begins the the ReadDirectoryChangesW operation
		//  (that operation must begin and be handled in the same thread when using completion routines)
		RestartMonitoring();
	}

    void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename)
    {
    }

	void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename)
	{
	}


    OnChangeCallback::~OnChangeCallback() {}
}

#else

namespace Utility
{
    void AttachFileSystemMonitor(StringSection<utf16> directoryName,
                                 StringSection<utf16> filename,
                                 std::shared_ptr<OnChangeCallback> callback) {

    }

    void AttachFileSystemMonitor(StringSection<utf8> directoryName,
                                 StringSection<utf8> filename,
                                 std::shared_ptr<OnChangeCallback> callback) {

    }

    void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename) {

    }

    void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename) {

    }

    void TerminateFileSystemMonitoring() {

    }

    OnChangeCallback::~OnChangeCallback() {}
}

#endif

