// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FileSystemMonitor.h"
#include "FileUtils.h"
#include "../StringUtils.h"

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
    
    OnChangeCallback::~OnChangeCallback() {}
    
    
    bool XlGetCurrentDirectory(uint32 nBufferLength, char lpBuffer[])
    {
        if (nBufferLength > 0) { lpBuffer[0] = '\0'; }
        return true;
    }
    
    bool XlGetCurrentDirectory(uint32 nBufferLength, ucs2 lpBuffer[])
    {
        if (nBufferLength > 0) { lpBuffer[0] = (ucs2)'\0'; }
        return true;
    }
    
    namespace RawFS
    {
        std::vector<std::string> FindFiles(const std::string& searchPath, FindFilesFilter::BitField filter)
        {
            return std::vector<std::string>();
        }
        
        bool DoesDirectoryExist(const char filename[])
        {
            return false;
        }

        void CreateDirectoryRecursive(StringSection<char> path) {}
        void CreateDirectoryRecursive(StringSection<utf8> path) {}
    }
    
}
