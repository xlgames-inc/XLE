// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Assets.h"
#include "AssetServices.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../ConsoleRig/Log.h"

#if 0
namespace std 
{
        // this is related to the hack to remove <mutex> from Assets.h for C++/CLR
    template unique_ptr<mutex>::~unique_ptr(); 
    template unique_ptr<recursive_mutex>::~unique_ptr(); 
}
#endif

namespace Assets 
{
    namespace Internal
    {
		/*
        void LogHeader(unsigned count, const char typeName[])
        {
            Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;
			Log(Verbose) << "    Asset set for type (" << typeName << ") with (" <<  count << ") items" << std::endl;
        }

        void LogAssetName(unsigned index, const char name[])
        {
			Log(Verbose) << "    [" << index << "] " << name << std::endl;
        }

        void InsertAssetName(   std::vector<std::pair<uint64, std::string>>& assetNames, 
                                uint64 hash, const std::string& name)
        {
            auto ni = LowerBound(assetNames, hash);
            if (ni != assetNames.cend() && ni->first == hash) {
                assert(0);  // hit a hash collision! In most cases, this will cause major problems!
                    // maybe this could happen if an asset is removed from the table, but it's name remains?
                ni->second = "<<collision>> " + ni->second + " <<with>> " + name;
            } else {
                assetNames.insert(ni, std::make_pair(hash, name));
            }
        }

        void InsertAssetNameNoCollision(   
            std::vector<std::pair<uint64, std::string>>& assetNames, 
            uint64 hash, const std::string& name)
        {
            auto ni = LowerBound(assetNames, hash);
            if (ni == assetNames.cend() || ni->first != hash)
                assetNames.insert(ni, std::make_pair(hash, name));
        }

        std::basic_string<ResChar> AsString() { return std::basic_string<ResChar>(); }


		std::unique_ptr<Threading::Mutex> CreateMutexPtr() { return std::make_unique<Threading::Mutex>(); }
        void LockMutex(Threading::Mutex& mutex) { mutex.lock(); }
        void UnlockMutex(Threading::Mutex& mutex) { mutex.unlock(); }

        std::unique_ptr<Threading::RecursiveMutex> CreateRecursiveMutexPtr() { return std::make_unique<Threading::RecursiveMutex>(); }
        void LockMutex(Threading::RecursiveMutex& mutex) { mutex.lock(); }
        void UnlockMutex(Threading::RecursiveMutex& mutex) { mutex.unlock(); }
		*/

    }

	AssetSetManager& GetAssetSetManager()
	{
		return Services::GetAssetSets();
	}
}
