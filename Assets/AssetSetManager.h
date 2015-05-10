// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include <memory>
#include <string>

namespace Assets
{
    class IAssetSet
    {
    public:
        virtual void            Clear() = 0;
        virtual void            LogReport() const = 0;
        virtual uint64          GetTypeCode() const = 0;
        virtual const char*     GetTypeName() const = 0;
        virtual unsigned        GetDivergentCount() const = 0;
        virtual uint64          GetDivergentId(unsigned index) const = 0;
        virtual bool            DivergentHasChanges(unsigned index) const = 0;
        virtual std::string     GetAssetName(uint64 id) const = 0;
        virtual ~IAssetSet();
    };

    class AssetSetManager
    {
    public:
        void Add(std::unique_ptr<IAssetSet>&& set);
        void Clear();
        void LogReport();
        unsigned BoundThreadId() const;
		bool IsBoundThread() const;

        unsigned GetAssetSetCount();
        const IAssetSet* GetAssetSet(unsigned index);

        AssetSetManager();
        ~AssetSetManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

