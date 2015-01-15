// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/Assets.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Streams/FileSystemMonitor.h"
#include "../Core/Exceptions.h"
#include <vector>
#include <algorithm>
#include <initializer_list>

namespace RenderCore
{
    using ::Assets::ResChar;

    #define VS_DefShaderModel   "vs_*"
    #define GS_DefShaderModel   "gs_*"
    #define PS_DefShaderModel   "ps_*"
    #define CS_DefShaderModel   "cs_*"
    #define HS_DefShaderModel   "hs_*"
    #define DS_DefShaderModel   "ds_*"

    class ShaderResId
    {
    public:
        ResChar     _filename[MaxPath];
        ResChar     _entryPoint[64];
        ResChar     _shaderModel[32];
        ShaderResId(const ResChar initializer[]);
        ShaderResId();
    };

    inline ShaderResId::ShaderResId(const ResChar initializer[])
    {
            // (convert unicode string -> single width char for entrypoint & shadermodel)

        const ResChar* endFileName = Utility::XlFindChar(initializer, ':');
        const ResChar* startShaderModel = nullptr;
        if (!endFileName) {
            XlCopyString(_filename, initializer);
            XlCopyString(_entryPoint, "main");
        } else {
            XlCopyNString(_filename, initializer, endFileName - initializer);
            startShaderModel = Utility::XlFindChar(endFileName+1, ':');

            if (!startShaderModel) {
                XlCopyString(_entryPoint, endFileName+1);
            } else {
                XlCopyNString(_entryPoint, endFileName+1, startShaderModel - endFileName - 1);
                XlCopyString(_shaderModel, startShaderModel+1);
            }
        }

        if (!startShaderModel) {
            XlCopyString(_shaderModel, PS_DefShaderModel);
        }
    }

    inline ShaderResId::ShaderResId()
    {
        _filename[0] = '\0'; _entryPoint[0] = '\0'; _shaderModel[0] = '\0';
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Used by the Metal interface to set many objects at once</summary>
    /// Some of the "RenderCore::Metal" interface functions allow the client to bind
    /// many of the same type of object to the pipeline, at the same time. For example,
    /// we might want to set multiple Metal::ShaderResourceView.
    ///
    /// Normally, ResourceList is constructed with MakeResourceList.
    /// <example>
    ///     For example:
    ///     <code>\code
    ///         RenderCore::Metal::DeviceContext& context = ...;
    ///         RenderCore::Metal::ShaderResourceView& redTexture = ...;
    ///         RenderCore::Metal::ShaderResourceView& blueTexture = ...;
    ///
    ///             // Set redTexture and blueTexture to pixel shader texture slots
    ///             // zero and one:
    ///         context.BindPS(MakeResourceList(redTexture, blueTexture));
    ///
    ///             // Set redTexture and blueTexture to vertex shader texture slots
    ///             // 10 and 11
    ///         context.BindVS(MakeResourceList(10, redTexture, blueTexture));
    ///     \endcode</code>
    /// </example>
    ///
    /// <seealso cref="RenderCore::MakeResourceList"/>
    template <typename Type, int Count>
        class ResourceList
        {
        public:
            ResourceList(const std::initializer_list<Type*>& initializers);
            ResourceList(unsigned offset, const std::initializer_list<Type*>& initializers);
            template<typename Tuple> ResourceList(const Tuple& initializers);
            template<typename Tuple> ResourceList(unsigned offset, const Tuple& initializers);

            typename Type::UnderlyingType       _buffers[Count];
            unsigned                            _startingPoint;

        private:
            template <int CountDown, int WriteTo, typename Tuple> struct Utility;
        };

    template <typename Type>
        class ResourceList<Type, 0>
        {
        public:
            ResourceList() { _buffers = nullptr; _startingPoint = 0; }

            typename Type::UnderlyingType*      _buffers;
            unsigned                            _startingPoint;
        };

    #pragma warning(push)
    #pragma warning(disable:4127)       // conditional expression is constant
    #pragma warning(disable:4718)       // recursive call has no side effects, deleting

    template <typename Type, int Count>
        ResourceList<Type,Count>::ResourceList(const std::initializer_list<Type*>& initializers)
        {
            size_t size = std::min(initializers.size(), size_t(Count));
            for (unsigned c=0; c<size; ++c) {
                _buffers[c] = initializers[c].GetUnderlying();
            }
            std::fill(&_buffers[size], &_buffers[Count], Type::UnderlyingType(0));
            _startingPoint = 0;
        }

    template <typename Type, int Count>
        ResourceList<Type,Count>::ResourceList(unsigned offset, const std::initializer_list<Type*>& initializers)
        {
            size_t size = std::min(initializers.size(), size_t(Count));
            for (unsigned c=0; c<size; ++c) {
                _buffers[c] = initializers[c].GetUnderlying();
            }
            std::fill(&_buffers[size], &_buffers[Count], Type::UnderlyingType(0));
            _startingPoint = offset;
        }

    template <typename Type, int Count>
        template <typename Tuple>
            ResourceList<Type,Count>::ResourceList(const Tuple& initializers)
        {
            const size_t size = (std::tuple_size<Tuple>::value < Count) ? (std::tuple_size<Tuple>::value) : Count;
            Utility<size, 0, Tuple>::InitializeFrom(_buffers, initializers);
            std::fill(&_buffers[size], &_buffers[Count], typename Type::UnderlyingType());
            _startingPoint = 0;
        }

    template <typename Type, int Count>
        template <typename Tuple>
            ResourceList<Type,Count>::ResourceList(unsigned offset, const Tuple& initializers)
        {
            const size_t size = (std::tuple_size<Tuple>::value < Count) ? (std::tuple_size<Tuple>::value) : Count;
            Utility<size, 0, Tuple>::InitializeFrom(_buffers, initializers);
            std::fill(&_buffers[size], &_buffers[Count], Type::UnderlyingType());
            _startingPoint = offset;
        }

    template <typename Type, int Count>
        template <int CountDown, int WriteTo, typename Tuple>
            struct ResourceList<Type,Count>::Utility<CountDown,WriteTo,Tuple>
            {
                static void InitializeFrom(typename Type::UnderlyingType buffers[], const Tuple& initializers)
                {
                    buffers[WriteTo] = std::get<WriteTo>(initializers).GetUnderlying();
                    Utility<CountDown-1, WriteTo+1, Tuple>::InitializeFrom(buffers, initializers);
                }
            };

    template <typename Type, int Count>
        template <int WriteTo, typename Tuple>
            struct ResourceList<Type,Count>::Utility<0,WriteTo,Tuple>
            {
                static void InitializeFrom(typename Type::UnderlyingType buffers[], const Tuple& initializers) {}
            };

    /// <summary>Constructs a new ResourceList object</summary>
    /// <seealso cref="RenderCore::ResourceList"/>
    template <typename Type>
        ResourceList<Type,1> MakeResourceList(const Type& zero) { return ResourceList<Type, 1>(std::make_tuple(std::ref(zero))); }

    template <typename Type>
        ResourceList<Type,2> MakeResourceList(const Type& zero, const Type& one) { return ResourceList<Type, 2>(std::make_tuple(std::ref(zero), std::ref(one))); }

    template <typename Type>
        ResourceList<Type,3> MakeResourceList(const Type& zero, const Type& one, const Type& two) { return ResourceList<Type, 3>(std::make_tuple(std::ref(zero), std::ref(one), std::ref(two))); }

    template <typename Type>
        ResourceList<Type,4> MakeResourceList(const Type& zero, const Type& one, const Type& two, const Type& three) { return ResourceList<Type, 4>(std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three))); }

    template <typename Type>
        ResourceList<Type,5> MakeResourceList(const Type& zero, const Type& one, const Type& two, const Type& three, const Type& four) { return ResourceList<Type, 5>(std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three), std::ref(four))); }

    template <typename Type>
        ResourceList<Type,6> MakeResourceList(const Type& zero, const Type& one, const Type& two, const Type& three, const Type& four, const Type& five) { return ResourceList<Type, 6>(std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three), std::ref(four), std::ref(five))); }

    template <typename Type>
        ResourceList<Type,7> MakeResourceList(const Type& zero, const Type& one, const Type& two, const Type& three, const Type& four, const Type& five, const Type& six) { return ResourceList<Type, 7>(std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three), std::ref(four), std::ref(five), std::ref(six))); }

    template <typename Type>
        ResourceList<Type,1> MakeResourceList(unsigned offset, const Type& zero) { return ResourceList<Type, 1>(offset, std::make_tuple(std::ref(zero))); }

    template <typename Type>
        ResourceList<Type,2> MakeResourceList(unsigned offset, const Type& zero, const Type& one) { return ResourceList<Type, 2>(offset, std::make_tuple(std::ref(zero), std::ref(one))); }

    template <typename Type>
        ResourceList<Type,3> MakeResourceList(unsigned offset, const Type& zero, const Type& one, const Type& two) { return ResourceList<Type, 3>(offset, std::make_tuple(std::ref(zero), std::ref(one), std::ref(two))); }

    template <typename Type>
        ResourceList<Type,4> MakeResourceList(unsigned offset, const Type& zero, const Type& one, const Type& two, const Type& three) { return ResourceList<Type, 4>(offset, std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three))); }

    template <typename Type>
        ResourceList<Type,5> MakeResourceList(unsigned offset, const Type& zero, const Type& one, const Type& two, const Type& three, const Type& four) { return ResourceList<Type, 5>(offset, std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three), std::ref(four))); }

    template <typename Type>
        ResourceList<Type,6> MakeResourceList(unsigned offset, const Type& zero, const Type& one, const Type& two, const Type& three, const Type& four, const Type& five) { return ResourceList<Type, 6>(offset, std::make_tuple(std::ref(zero), std::ref(one), std::ref(two), std::ref(three), std::ref(four), std::ref(five))); }


    #pragma warning(pop)
}

