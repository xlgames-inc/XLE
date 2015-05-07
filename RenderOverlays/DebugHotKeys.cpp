// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DebugHotKeys.h"
#include "DebuggingDisplay.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/PtrUtils.h"

namespace RenderOverlays
{

    class HotKeyInputHandler : public DebuggingDisplay::IInputListener
    {
    public:
        bool    OnInputEvent(const DebuggingDisplay::InputSnapshot& evnt);

        HotKeyInputHandler(const std::string& filename) : _filename(filename) {}
    protected:
        std::string _filename;
    };

    class TableOfKeys
    {
    public:
        TableOfKeys(const char filename[]);
        const std::vector<std::pair<uint32, std::string>>& GetTable() const { return _table; }

        const std::shared_ptr<Assets::DependencyValidation>& GetDependencyValidation() const     
            { return _validationCallback; }

    private:
        std::shared_ptr<Assets::DependencyValidation>   _validationCallback;
        std::vector<std::pair<uint32, std::string>>         _table;
    };

    TableOfKeys::TableOfKeys(const char filename[])
    {
        size_t fileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);

        Data data;
        data.Load((const char*)sourceFile.get(), int(fileSize));

        std::vector<std::pair<uint32, std::string>> table;
        for (int c=0; c<data.Size(); ++c) {
            auto child = data.ChildAt(c);
            if (!child) continue;

            auto executeString = child->ChildAt(0);
            if (executeString) {
                const char* keyName = child->StrValue();
                auto p = std::make_pair(
                    RenderOverlays::DebuggingDisplay::KeyId_Make(keyName),
                    std::string(executeString->StrValue()));
                table.push_back(p);
            }
        }


        auto validationCallback = std::make_shared<Assets::DependencyValidation>();
        Assets::RegisterFileDependency(validationCallback, filename);

        _table = std::move(table);
        _validationCallback = std::move(validationCallback);
    }

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

