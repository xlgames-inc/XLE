// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "../ConsoleRig/OutputStream.h"     // for xleWarning
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <algorithm>

namespace SceneEngine
{

    ///////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        struct IBoxTable { virtual ~IBoxTable(); };
        extern std::vector<std::unique_ptr<IBoxTable>> BoxTables;

        template <typename Box> struct BoxTable : public IBoxTable
        {
            std::vector<std::pair<uint64, std::unique_ptr<Box>>>    _internalTable;
        };
    }

    template <typename Box> std::vector<std::pair<uint64, std::unique_ptr<Box>>>& GetBoxTable()
        {
            static Internal::BoxTable<Box>* table = nullptr;
            if (!table) {
                auto t = std::make_unique<Internal::BoxTable<Box>>();
                table = t.get();    // note -- this will end up holding a dangling ptr after calling shutdown (could use a weak_ptr...?)
                Internal::BoxTables.push_back(std::move(t));
            }
            return table->_internalTable;
        }

    template <typename Box> Box& FindCachedBox(const typename Box::Desc& desc)
    {
        uint64 hashValue = Hash64(&desc, PtrAdd(&desc, sizeof(typename Box::Desc)));
        auto& boxTable = GetBoxTable<Box>();
        auto i = std::lower_bound(boxTable.cbegin(), boxTable.cend(), hashValue, CompareFirst<uint64, std::unique_ptr<Box>>());
        if (i!=boxTable.cend() && i->first==hashValue) {
            return *i->second;
        }

        auto ptr = std::make_unique<Box>(desc);
        ConsoleRig::xleWarningDebugOnly("Created cached box for type (%s) -- first time. HashValue:(0x%08x%08x)\n", 
                                        typeid(Box).name(), uint32(hashValue>>32), uint32(hashValue));
        auto i2 = boxTable.insert(i, std::make_pair(hashValue, std::move(ptr)));
        return *i2->second;
    }

    template <typename Desc> uint64 CalculateCachedBoxHash(const Desc& desc)
    {
        return Hash64(&desc, PtrAdd(&desc, sizeof(Desc)));
    }

    template <typename Box> Box& FindCachedBoxDep(const typename Box::Desc& desc)
    {
        auto hashValue = CalculateCachedBoxHash(desc);
        auto& boxTable = GetBoxTable<Box>();
        auto i = std::lower_bound(boxTable.begin(), boxTable.end(), hashValue, CompareFirst<uint64, std::unique_ptr<Box>>());
        if (i!=boxTable.end() && i->first==hashValue) {
            if (i->second->GetDependancyValidation().GetValidationIndex()!=0) {
                i->second = std::make_unique<Box>(desc);
                ConsoleRig::xleWarningDebugOnly("Created cached box for type (%s) -- rebuilding due to validation failure. HashValue:(0x%08x%08x)\n", 
                                                typeid(Box).name(), uint32(hashValue>>32), uint32(hashValue));
            }
            return *i->second;
        }

        auto ptr = std::make_unique<Box>(desc);
        ConsoleRig::xleWarningDebugOnly(    "Created cached box for type (%s) -- first time. HashValue:(0x%08x%08x)\n", 
                                            typeid(Box).name(), uint32(hashValue>>32), uint32(hashValue));
        auto i2 = boxTable.insert(i, std::make_pair(hashValue, std::move(ptr)));
        return *i2->second;
    }

    void ResourceBoxes_Shutdown();

    ///////////////////////////////////////////////////////////////////////////////////////////////

}

