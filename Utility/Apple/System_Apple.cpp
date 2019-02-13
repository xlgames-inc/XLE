
#include "SystemUtils.h"
#include "../../Core/SelectConfiguration.h"
#include "../../Core/Types.h"

#if PLATFORMOS_TARGET != PLATFORMOS_ANDROID

#include <mach/mach_time.h>
#include <pthread/pthread.h>

#if PLATFORMOS_TARGET == PLATFORMOS_OSX
#include "Mutex.h"

#include <libgen.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>

#include <unordered_map>
#include <unordered_set>
#include <thread>
#endif

namespace Utility
{
    uint32 XlGetCurrentThreadId()
    {
        return pthread_mach_thread_np(pthread_self());
    }

    uint64 GetPerformanceCounter()
    {
        return mach_absolute_time();
    }
    
    uint64 GetPerformanceCounterFrequency()
    {
        mach_timebase_info_data_t tbInfo;
        mach_timebase_info(&tbInfo);
        return tbInfo.denom * 1000000000 / tbInfo.numer;
    }
}

#include "FileSystemMonitor.h"
#include "FileUtils.h"
#include "../StringUtils.h"

namespace Utility
{
#if PLATFORMOS_TARGET == PLATFORMOS_OSX
    struct MonitorEntry {
        std::string filepath;
        int fd;
        bool is_dir;
        std::vector<std::weak_ptr<OnChangeCallback>> callbacks;
    };
    
    template<typename T>
    static std::string FilePathFromDirectoryNameAndFileName(StringSection<T> directoryName, StringSection<T> filename) {
        std::basic_string<T> filepath = directoryName.begin();
        if (filepath.length() > 0 && filename.size() > 0) {
            filepath += u("/");
        }
        filepath += filename.begin();
        const char *cpath = reinterpret_cast<const char *>(filepath.c_str());
        auto canonical_path = std::make_unique<char[]>(PATH_MAX);
        realpath(cpath, canonical_path.get());
        return canonical_path.get();
    }
    
    class KQueueMonitor {
    private:
        // Identifier for our kernel queue, retrived from a call to kqueue()
        int _kq;
        
        // Maps open file descriptions to the filepath opened
        std::unordered_map<int, std::string> _fd_to_path;
        
        // Maps filepaths to their monitor entries
        std::unordered_map<std::string, MonitorEntry> _entries;
        
        // Tracks the filepaths of deleted and not yet existing files that
        // we have been asked to track
        std::unordered_set<std::string> _deleted_files;
        
        // Lock for working with any property of this class
        Threading::RecursiveMutex _lock;
        
        // Monitor thread
        std::thread _thread;
        
        // When true, the monitor thread should end
        bool _shouldStop;
        
        // Gets the MonitorEntry for a given file description, it one exists
        std::optional<MonitorEntry*> EntryForFD(int fd);
        
        // Scans the directory at the given filepath and tracks new files if they are in
        // _deleted_files
        void DirScan(StringSection<char> path);
        
        // Runs the callbacks for the given entry
        void RunCallbacks(MonitorEntry &entry);
        
        // Loop for the monitor thread
        void MonitorLoop();
    public:
        KQueueMonitor();
        ~KQueueMonitor();
        
        // Start monitoring the given filepath and run the given callback when it changes
        // This will add another callback if the filepath is already being monitored
        void Monitor(StringSection<char> filepath, std::shared_ptr<OnChangeCallback> callback);
        
        // Simulates what would happen if the given filepath was changed
        void FakeFileChange(StringSection<char> filepath);
    };
    
    std::optional<MonitorEntry*> KQueueMonitor::EntryForFD(int fd) {
        auto it = _fd_to_path.find(fd);
        if (it == _fd_to_path.end()) {
            return {};
        }
        auto maybeEntry = _entries.find(it->second);
        assert(maybeEntry != _entries.end());
        return &maybeEntry->second;
    }
    
    // Loop that watches for any file changes and triggers callbacks
    void KQueueMonitor::MonitorLoop() {
        while (!_shouldStop) {
            kevent64_s change;
            
            // Timeout is 10ms. After that it checks if the thread
            // should exit (the application has been closed)
            timespec t;
            t.tv_sec = 0;
            t.tv_nsec = 10 * 1000; // 10ms
            int num_changes = kevent64(_kq, NULL, 0, &change, 1, 0, &t);
            if (num_changes > 0) {
                int fd = static_cast<int>(change.ident);
                ScopedLock(_lock);
                auto maybeEntry = EntryForFD(fd);
                if (maybeEntry) {
                    MonitorEntry &entry = **maybeEntry;
                    if (!entry.is_dir) {
                        // If it's not a directory, we check if it's been written to
                        // or if it's been deleted
                        if (change.fflags & NOTE_DELETE && entry.fd != 0) {
                            // The file has been deleted, let's remove references to it,
                            // close the file descriptor, and trigger a directory search in
                            // case a new file with the same name has been created
                            _fd_to_path.erase(entry.fd);
                            close(entry.fd);
                            entry.fd = 0;
                            _deleted_files.emplace(entry.filepath);
                            
                            // Trigger search
                            auto dname = std::make_unique<char[]>(PATH_MAX);
                            dirname_r(entry.filepath.c_str(), dname.get());
                            DirScan(dname.get());
                        } else {
                            // The file has been written to. Let's trigger the callbacks for it
                            RunCallbacks(entry);
                        }
                    } else {
                        // A directory has been changed, let's trigger a search of it
                        DirScan(entry.filepath);
                    }
                }
            }
        }
    }
    
    void KQueueMonitor::DirScan(StringSection<char> path) {
        ScopedLock(_lock);
        auto entryIt = _entries.find(path.begin());
        if (entryIt == _entries.end()) {
            return;
        }
        auto &entry = entryIt->second;
        // List the dir and look for new files
        DIR *dir = opendir(entry.filepath.c_str());
        if (dir != NULL) {
            dirent *d_entry;
            while ((d_entry = readdir(dir)) != NULL) {
                // Skip "." and ".." entries
                if (strcmp(d_entry->d_name, ".") == 0 || strcmp(d_entry->d_name, "..") == 0) {
                    continue;
                }
                
                // Construct the canonical path
                std::string full_path;
                if (entry.filepath.size()) {
                    full_path = entry.filepath + "/" + d_entry->d_name;
                    auto buffer = std::make_unique<char[]>(PATH_MAX);
                    realpath(full_path.c_str(), buffer.get());
                    full_path = buffer.get();
                } else {
                    full_path = d_entry->d_name;
                }
                
                // Check if this is a new file that we care about
                auto it = _deleted_files.find(full_path);
                if (it != _deleted_files.end()) {
                    // One of the files we've been tracking has been created
                    // Let's start tracking it too
                    _deleted_files.erase(it);
                    Monitor(full_path, nullptr);
                    auto entry = _entries[full_path];
                    if (entry.fd) {
                        RunCallbacks(entry);
                    }
                }
            }
        }
        closedir(dir);
    }
    
    void KQueueMonitor::RunCallbacks(MonitorEntry &entry) {
        for (unsigned i = 0; i < entry.callbacks.size(); ++i) {
            auto weakCallback = entry.callbacks[i];
            auto callback = weakCallback.lock();
            if (callback) {
                callback->OnChange();
            } else {
                // The callback no longer exists, let's remove it
                entry.callbacks.erase(entry.callbacks.begin() + i);
                --i;
            }
        }
    }
    
    KQueueMonitor::KQueueMonitor() : _kq(kqueue()),
                                     _shouldStop(false),
                                     _thread(&KQueueMonitor::MonitorLoop, this) {
        assert(_kq != -1);
    }
    
    KQueueMonitor::~KQueueMonitor() {
        _shouldStop = true;
        for (auto &pair : _fd_to_path) {
            close(pair.first);
        }
        _thread.join();
    }
    
    void KQueueMonitor::Monitor(StringSection<char> filepath, std::shared_ptr<OnChangeCallback> callback) {
        ScopedLock(_lock);
        const char *cpath = filepath.begin();
        auto entryIt = _entries.find(cpath);
        
        // Create the entry if it doesn't already exist
        if (entryIt == _entries.end()) {
            _entries[cpath] = {};
            _entries[cpath].filepath = cpath;
            _entries[cpath].fd = 0;
            
            // Check if this path is a directory
            DIR *dir = opendir(cpath);
            _entries[cpath].is_dir = dir != NULL;
            if (dir) {
                closedir(dir);
            }
        }
        
        // Add the callback if given
        MonitorEntry &entry = _entries[cpath];
        if (callback) {
            entry.callbacks.push_back(callback);
        }
        if (entry.fd != 0) {
            // We are already tracking this file, so we don't need
            // to register it
            return;
        }
        
        // Start tracking the file
        int fd = open(cpath, O_EVTONLY);
        if (fd != -1) {
            entry.fd = fd;
            _fd_to_path[fd] = cpath;
            kevent64_s new_event;
            new_event.ident = fd;
            new_event.filter = EVFILT_VNODE;
            new_event.flags = EV_ADD | EV_CLEAR | EV_ENABLE;
            new_event.fflags = NOTE_WRITE | (entry.is_dir ? 0 : NOTE_DELETE);
            new_event.data = 0;
            new_event.udata = 0;
            // Register that we are tracking this file with the kernel
            kevent64(_kq, &new_event, 1, NULL, 0, 0, NULL);
        } else {
            // This file doesn't exist yet, add it to the list
            // of files to check for when a directory is updated
            _deleted_files.emplace(cpath);
        }
    }
    
    void KQueueMonitor::FakeFileChange(StringSection<char> filepath) {
        ScopedLock(_lock);
        auto maybeEntry = _entries.find(filepath.begin());
        if (maybeEntry == _entries.end()) {
            return;
        }
        RunCallbacks(maybeEntry->second);
    }
    
    static KQueueMonitor s_kq_monitor;
    
    void AttachFileSystemMonitor(StringSection<utf16> directoryName,
                                 StringSection<utf16> filename,
                                 std::shared_ptr<OnChangeCallback> callback) {
    }
    
    void AttachFileSystemMonitor(StringSection<utf8> directoryName,
                                 StringSection<utf8> filename,
                                 std::shared_ptr<OnChangeCallback> callback) {
        // First monitor the directory
        const char *cpath = reinterpret_cast<const char *>(directoryName.begin());
        auto canonical_path = std::make_unique<char[]>(PATH_MAX);
        realpath(cpath, canonical_path.get());
        s_kq_monitor.Monitor(canonical_path.get(), nullptr);
        
        // Now monitor the file itself
        auto filepath = FilePathFromDirectoryNameAndFileName(directoryName, filename);
        s_kq_monitor.Monitor(filepath, callback);
    }
    
    void    FakeFileChange(StringSection<utf16> directoryName, StringSection<utf16> filename) {
        
    }
    
    void    FakeFileChange(StringSection<utf8> directoryName, StringSection<utf8> filename) {
        auto filepath = FilePathFromDirectoryNameAndFileName(directoryName, filename);
        s_kq_monitor.FakeFileChange(filepath);
    }
    
    OnChangeCallback::~OnChangeCallback() {}
#else
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

    OnChangeCallback::~OnChangeCallback() {}
#endif
}

#else

namespace Utility
{
    uint32 XlGetCurrentThreadId()
    {
        return 0;
    }

    static const auto NSEC_PER_SEC = 1000000000ull;

    uint64 GetPerformanceCounter()
    {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        return (uint64)(t.tv_sec) * NSEC_PER_SEC + (uint64)(t.tv_nsec);
    }

    uint64 GetPerformanceCounterFrequency()
    {
        return NSEC_PER_SEC;
    }
}

#endif

namespace Utility
{
    XlHandle XlCreateEvent(bool manualReset) { return 0; }
    bool XlResetEvent(XlHandle event) { return false; }
    bool XlSetEvent(XlHandle event) { return false; }
    bool XlCloseSyncObject(XlHandle object) { return false; }
    uint32 XlWaitForSyncObject(XlHandle object, uint32 waitTime) { return 0; }
    uint32 XlWaitForMultipleSyncObjects(uint32 waitCount, XlHandle waitObjects[], bool waitAll, uint32 waitTime, bool alterable) { return 0; }

    bool XlGetCurrentDirectory(uint32 dim, char dst[])
    {
        if (dim > 0) dst[0] = '\0';
        return false;
    }
    bool XlGetCurrentDirectory(uint32 dim, ucs2 dst[])
    {
        if (dim > 0) dst[0] = '\0';
        return false;
    }
    uint64 XlGetCurrentFileTime() { return 0; }

    void XlGetProcessPath(utf8 dst[], size_t bufferCount)
    {
        if (bufferCount > 0) dst[0] = '\0';
    }

    void XlGetProcessPath(ucs2 dst[], size_t bufferCount)
    {
        if (bufferCount > 0) dst[0] = '\0';
    }

    void XlChDir(const utf8 path[]) {}
    void XlChDir(const ucs2 path[]) {}

    const char* XlGetCommandLine() { return ""; }

    ModuleId GetCurrentModuleId() { return 0; }

}


