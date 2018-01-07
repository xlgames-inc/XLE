// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DebugHotKeys.h"
#include "DebuggingDisplay.h"
#include "../Assets/Assets.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/ConfigFileContainer.h"
#include "../Assets/AssetUtils.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/Conversion.h"

namespace RenderOverlays
{

    class HotKeyInputHandler : public DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(const DebuggingDisplay::InputSnapshot& evnt);

        HotKeyInputHandler(const ::Assets::rstring& filename) : _filename(filename) {}
    protected:
        ::Assets::rstring _filename;
    };

    class TableOfKeys
    {
    public:
        const std::vector<std::pair<uint32, std::string>>& GetTable() const { return _table; }
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _validationCallback; }

        TableOfKeys(
			InputStreamFormatter<utf8>& formatter,
			const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal);
        ~TableOfKeys();
    private:
        ::Assets::DepValPtr                             _validationCallback;
        std::vector<std::pair<uint32, std::string>>     _table;
    };

    TableOfKeys::TableOfKeys(
		InputStreamFormatter<utf8>& formatter,
		const ::Assets::DirectorySearchRules&,
		const ::Assets::DepValPtr& depVal)
	: _validationCallback(depVal)
    {
        Document<InputStreamFormatter<utf8>> doc(formatter);

        auto attrib = doc.FirstAttribute();
        while (attrib) {

            auto executeString = attrib.Value();
            if (!executeString.IsEmpty()) {
                auto keyName = attrib.Name();
                auto p = std::make_pair(
                    RenderOverlays::DebuggingDisplay::KeyId_Make(
                        StringSection<char>((const char*)keyName.begin(), (const char*)keyName.end())),
                    Conversion::Convert<std::string>(executeString.AsString()));
                _table.push_back(p);
            }

            attrib = attrib.Next();
        }
    }
    TableOfKeys::~TableOfKeys() {}

    bool    HotKeyInputHandler::OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
    {
        using namespace RenderOverlays::DebuggingDisplay;
    
        static KeyId ctrlKey = KeyId_Make("control");
        if (evnt.IsHeld(ctrlKey)) {
            auto& t = Assets::GetAssetDep<TableOfKeys>(_filename.c_str());  // (todo -- cash the hash value, rather than rebuilding every time)
            for (auto i=t.GetTable().cbegin(); i!=t.GetTable().cend(); ++i) {
                if (evnt.IsPress(i->first)) {
                    ConsoleRig::Console::GetInstance().Execute(i->second);
                    return true;
                }
            }
        }

        return false;
    }

    std::unique_ptr<DebuggingDisplay::IInputListener> MakeHotKeysHandler(const char filename[])
    {
        return std::make_unique<HotKeyInputHandler>(filename);
    }

}

