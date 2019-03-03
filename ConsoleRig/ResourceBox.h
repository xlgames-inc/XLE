// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Log.h"
#include "../../Core/Types.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <vector>
#include <algorithm>

namespace ConsoleRig
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

		template <typename Box> std::vector<std::pair<uint64, std::unique_ptr<Box>>>& GetBoxTable()
        {
            static Internal::BoxTable<Box>* table = nullptr;
            if (!table) {
                auto t = std::make_unique<Internal::BoxTable<Box>>();
                table = t.get();    // note -- this will end up holding a dangling ptr after calling shutdown (could use a weak_ptr...?)
                Internal::BoxTables.emplace_back(std::move(t));
            }
            return table->_internalTable;
        }
    }

	template <typename Desc> uint64 CalculateCachedBoxHash(const Desc& desc)
	{
		return Hash64(&desc, PtrAdd(&desc, sizeof(Desc)));
	}

    template <typename Box> Box& FindCachedBox(const typename Box::Desc& desc)
    {
        auto hashValue = CalculateCachedBoxHash(desc);
        auto& boxTable = Internal::GetBoxTable<Box>();
        auto i = LowerBound(boxTable, hashValue);
        if (i!=boxTable.cend() && i->first==hashValue) {
            return *i->second;
        }

        auto ptr = std::make_unique<Box>(desc);
		Log(Verbose) << "Created cached box for type (" << typeid(Box).name() << ") -- first time. HashValue:(0x" << std::hex << hashValue << std::dec << ")" << std::endl;
        auto i2 = boxTable.emplace(i, std::make_pair(hashValue, std::move(ptr)));
        return *i2->second;
    }

    template <typename Box, typename... Params> Box& FindCachedBox2(Params... params)
    {
        return FindCachedBox<Box>(typename Box::Desc(std::forward<Params>(params)...));
    }

    template <typename Box> Box& FindCachedBoxDep(const typename Box::Desc& desc)
    {
        auto hashValue = CalculateCachedBoxHash(desc);
        auto& boxTable = Internal::GetBoxTable<Box>();
        auto i = LowerBound(boxTable, hashValue);
        if (i!=boxTable.end() && i->first==hashValue) {
            if (i->second->GetDependencyValidation()->GetValidationIndex()!=0) {
                i->second = std::make_unique<Box>(desc);
				Log(Verbose) << "Created cached box for type (" << typeid(Box).name() << ") -- rebuilding due to validation failure. HashValue:(0x" << std::hex << hashValue << std::dec << ")" << std::endl;
            }
            return *i->second;
        }

        auto ptr = std::make_unique<Box>(desc);
		Log(Verbose) << "Created cached box for type (" << typeid(Box).name() << ") -- first time. HashValue:(0x" << std::hex << hashValue << std::dec << ")" << std::endl;
        auto i2 = boxTable.emplace(i, std::make_pair(hashValue, std::move(ptr)));
        return *i2->second;
    }

    template <typename Box, typename... Params> Box& FindCachedBoxDep2(Params... params)
    {
        return FindCachedBoxDep<Box>(Box::Desc(std::forward<Params>(params)...));
    }

    void ResourceBoxes_Shutdown();

    ///////////////////////////////////////////////////////////////////////////////////////////////

}

