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

    namespace Internal { template <typename AssetType> class AssetSet; }

    class AssetSetManager
    {
    public:
        template<typename Type>
            Internal::AssetSet<Type>* GetSetForType();

        void Clear();
        void LogReport();
        unsigned BoundThreadId() const;
        bool IsBoundThread() const;

        unsigned GetAssetSetCount();
        const IAssetSet* GetAssetSet(unsigned index);

        void Lock();
        void Unlock();

        AssetSetManager();
        ~AssetSetManager();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        IAssetSet* GetSetForTypeCode(size_t typeCode);
        void Add(size_t typeCode, std::unique_ptr<IAssetSet>&& set);
    };

    template<typename Type>
        Internal::AssetSet<Type>* AssetSetManager::GetSetForType()
    {
            // Try once, without locking
            // once the "GetSetForTypeCode" returns something other than
            // null, then we can consider that value permanent. No other
            // thread can ever change it. The only exception is when shutting down.
        auto* existing = GetSetForTypeCode(typeid(Type).hash_code());
        if (existing) {
                // we have to force an up-cast here...
            return static_cast<Internal::AssetSet<Type>*>(existing);
        }

            // if it doesn't exist...
            // lock, and try again (from the start)
        Lock();
        existing = GetSetForTypeCode(typeid(Type).hash_code());
        if (existing) {
            Unlock();
            return static_cast<Internal::AssetSet<Type>*>(existing);
        }

        Internal::AssetSet<Type>* result = nullptr;
        try 
        {
            auto newPtr = std::make_unique<Internal::AssetSet<Type>>();
            result = newPtr.get();
            Add(typeid(Type).hash_code(), std::move(newPtr));
        } catch (...) {
            Unlock();
            throw;
        }
        Unlock();
        return result;
    }

}

