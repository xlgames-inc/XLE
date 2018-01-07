// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if 0

#include <vector>
#include <string>
#include <utility>

namespace Assets { namespace Internal 
{
	void LogHeader(unsigned count, const char typeName[]);
    void LogAssetName(unsigned index, const char name[]);
    void InsertAssetName(   
        std::vector<std::pair<uint64_t, std::string>>& assetNames, 
        uint64_t hash, const std::string& name);
    void InsertAssetNameNoCollision(   
        std::vector<std::pair<uint64_t, std::string>>& assetNames, 
        uint64_t hash, const std::string& name);
}}

#endif