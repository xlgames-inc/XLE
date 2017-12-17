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
    
}
