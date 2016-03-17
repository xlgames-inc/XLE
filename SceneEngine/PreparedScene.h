// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "../Utility/MiniHeap.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

namespace SceneEngine
{
    class PreparedScene
    {
    public:
        using Id = uint64;

        template<typename Type, typename... Args> Type* Allocate(Id id, Args... args);
        template<typename Type> Type* Get(Id id = 0);

        PreparedScene();
        ~PreparedScene();
    private:
        MiniHeap _heap;

        class Block
        {
        public:
            Id _id;
            MiniHeap::Allocation _allocation;
        };
        std::vector<std::pair<size_t, Block>> _blocks;
    };

    template<typename Type, typename... Args> 
        Type* PreparedScene::Allocate(Id id, Args... args)
        {
            auto r = std::equal_range(
                _blocks.begin(), _blocks.end(),
                typeid(Type).hash_code(), CompareFirst<size_t, Block>());
            for (auto i=r.first; i!=r.second; ++i) {
                assert(i->second._id != id);
            }

            #pragma push_macro("new")
            #undef new
                auto alloc = _heap.Allocate(sizeof(Type));
                new(alloc._allocation) Type(args...);
            #pragma pop_macro("new")

            _blocks.insert(r.second, std::make_pair(typeid(Type).hash_code(), Block{ id, alloc }));
            return (Type*)alloc._allocation;
        }

    template<typename Type> 
        Type* PreparedScene::Get(Id id)
        {
            auto r = std::equal_range(
                _blocks.begin(), _blocks.end(),
                typeid(Type).hash_code(), CompareFirst<size_t, Block>());
            for (auto i=r.first; i!=r.second; ++i)
                if (i->second._id == id)
                    return (Type*)i->second._allocation._allocation;
            return nullptr;
        }
}

