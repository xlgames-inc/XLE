// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../FunctionUtils.h"
#include "../ParameterBox.h"        // only needed for TrySet
#include <string>
#include <functional>
#include <vector>

namespace Utility
{
    namespace ImpliedTyping { class TypeDesc; }
    namespace Internal { class ClassAccessorsHelper; }

    /// <summary>Get and set properties associated with a native C++ class</summary>
    /// This class is designed to facilite interaction between a native class and data.
    /// For a class type, we can define get and set operations, with a given string name. The
    /// system can use these properties for serialisation tasks (as well as binding to data
    /// in various ways.
    ///
    /// To use this system, classes must define a specialisation of GetAccessors<Type>().
    /// That exposes a list of get and set operations to the system. Typically these get
    /// and set operations will use the default implementations. But sometimes custom
    /// functionality can be provided for special types.
    ///
    /// <example>
    ///     Here is a typical implementation of GetAccessors<>()
    ///      <code>\code
    ///         class Rect2D
    ///         {
    ///             UInt2 _topLeft, _bottomRight;
    //              friend const ClassAccessors& GetAccessors<Rect2D>();
    ///         };
    ///
    ///         template<> const ClassAccessors& GetAccessors<Rect2D>()
    ///         {
    ///             static ClassAccessors accessors(typeid(Rect2D).hash_code());
    ///             static bool init = false;
    ///             if (!init) {
    ///                 accessors.Add(
    ///                     u("TopLeft"), 
    ///                     DefaultGet(Rect2D, _topLeft),
    ///                     DefaultSet(Rect2D, _topLeft));
    ///                 accessors.Add(
    ///                     u("BottomRight"), 
    ///                     DefaultGet(Rect2D, _bottomRight),
    ///                     DefaultSet(Rect2D, _bottomRight));
    ///                 accessors.Add(
    ///                     u("Size"), 
    ///                     [](const Rect2D& r) { return UInt2(r._bottomRight - r._topLeft); },
    ///                     nullptr);
    ///                 init = true;
    ///             }
    ///             return accessors;
    ///         };
    ///     \endcode</code>
    ///     Notice that some accessors provide access to a member. The "Size" accessor has a special case "get"
    ///     implementation. We could have provided a "set" for "Size" as well -- but in this case it's ommitted.
    /// </example>
    ///
    /// Classes with registered accessors can be used with the AccessorSerialize and AccessorDeserialize functions.
    /// These can load and save these types automatically.
    ///
    /// This system is particularly useful for types that require multiple different types of serialisation methods
    /// (or multiple data-based ways to interact with them). For example, the editor performs string based get and sets
    /// on native types. At the same type, we may want to have separate code to load and save those types from disk.
    /// This is a perfect type to use class accessors.
    class ClassAccessors
    {
    public:

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        class Property
        {
        public:
            using CastFromFn        = std::function<bool(void*, const void*, ImpliedTyping::TypeDesc, bool)>;
            using CastFromArrayFn   = std::function<bool(void*, size_t, const void*, ImpliedTyping::TypeDesc, bool)>;
            using CastToFn          = std::function<bool(const void*, void*, size_t, ImpliedTyping::TypeDesc, bool)>;
            using CastToArrayFn     = std::function<bool(const void*, size_t, void*, size_t, ImpliedTyping::TypeDesc, bool)>;

            std::string                 _name;
            CastFromFn                  _castFrom;
            CastFromArrayFn             _castFromArray;
            CastToFn                    _castTo;
            CastToArrayFn               _castToArray;
            ImpliedTyping::TypeDesc     _naturalType;
            size_t                      _fixedArrayLength;
        };

        class ChildList
        {
        public:
            using CreateFn      = std::function<void*(void*)>;
            using GetCountFn    = std::function<size_t(const void*)>;
            using GetByIndexFn  = std::function<const void*(const void*,size_t)>;
            using GetByKeyFn    = std::function<const void*(const void*,uint64)>;

            std::string                 _name;
            const ClassAccessors*       _childProps;
            CreateFn                    _createFn;
            GetCountFn                  _getCount;
            GetByIndexFn                _getByIndex;
            GetByKeyFn                  _getByKeyFn;
        };

        size_t GetPropertyCount() const { return _properties.size(); }
        const Property& GetPropertyByIndex(size_t index) const { return _properties[index].second; }

        size_t GetChildListCount() const { return _childLists.size(); }
        const ChildList& GetChildListByIndex(size_t index) const { return _childLists[index].second; }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
                ////   Q U E R I E S   I N T E R F A C E   ////

        bool TryOpaqueGet(
            void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType,
            const void* src, uint64 id,
            bool stringForm = false) const;

        template<typename ResultType, typename Type>
            bool TryGet(ResultType& result, const Type& src, uint64 id) const;

        template<typename ResultType>
            bool TryGet(ResultType& result, const void* src, uint64 id) const;

        bool TryOpaqueSet(
            void* dst, uint64 id,
            IteratorRange<const void*> src, ImpliedTyping::TypeDesc srcType,
            bool stringForm = false) const;

        bool TryOpaqueSet(
            void* dst, uint64 id, size_t arrayIndex,
            IteratorRange<const void*> src, ImpliedTyping::TypeDesc srcType,
            bool stringForm = false) const;

        bool TryGetNaturalType(ImpliedTyping::TypeDesc& result, uint64 id) const;
        
        template<typename Type>
            bool TrySet(
                Type& dst, uint64 id,
                IteratorRange<const void*> src, ImpliedTyping::TypeDesc srcType,
                bool stringForm = false) const;

        template<typename ValueType, typename Type>
            bool TrySet(const ValueType& valueSrc, Type& dst, uint64 id) const;
            
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
                ////   C H I L D   I N T E R F A C E   ////

        std::pair<void*, const ClassAccessors*> TryCreateChild(void* dst, uint64 childListId) const;

        template<typename Type>
            std::pair<void*, const ClassAccessors*> TryCreateChild(Type& dst, uint64 childListId) const;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
                ////   I N I T I A L I Z A T I O N   I N T E R F A C E   ////

        template<typename GetFn, typename SetFn>
            void Add(const char name[], GetFn&& getter, SetFn&& setter, const ImpliedTyping::TypeDesc& naturalType = ImpliedTyping::TypeCat::Void, size_t fixedArrayLength = 1);

        template<typename ChildType, typename CreateFn, typename GetCountFn, typename GetByIndexFn, typename GetByKeyFn>
            void AddChildList(const char name[], CreateFn&&, GetCountFn&&, GetByIndexFn&&, GetByKeyFn&&);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ClassAccessors(size_t associatedType);
        ~ClassAccessors();

    protected:
        size_t _associatedType;

        Property& PropertyForId(uint64 id);

        VariantFunctions _getters;
        VariantFunctions _setters;
        std::vector<std::pair<uint64, Property>> _properties;
        std::vector<std::pair<uint64, ChildList>> _childLists;

        friend Internal::ClassAccessorsHelper;
    };

    template<typename Type>
        bool ClassAccessors::TrySet(
            Type& dst,
            uint64 id, IteratorRange<const void*> src,
            ImpliedTyping::TypeDesc srcType, bool stringForm) const
        {
            assert(typeid(Type).hash_code() == _associatedType);
            return TryCastFrom(&dst, id, src, srcType, stringForm);
        }

    template<typename ValueType, typename Type>
        bool ClassAccessors::TrySet(const ValueType& valueSrc, Type& dst, uint64 id) const
        {
            assert(typeid(Type).hash_code() == _associatedType);
            return TryCastFrom(dst, id, &valueSrc, ImpliedTyping::TypeOf<ValueType>());
        }
        
    template<typename Type>
        std::pair<void*, const ClassAccessors*> ClassAccessors::TryCreateChild(
            Type& dst, uint64 childListId) const
        {
            assert(typeid(Type).hash_code() == _associatedType);
            return TryCreateChild(&dst, childListId);
        }

    template<typename ResultType, typename Type>
        bool ClassAccessors::TryGet(ResultType& result, const Type& src, uint64 id) const
        {
            return _getters.TryCall<ResultType, const Type&>(result, id, src);
        }
}

template<typename Type> const Utility::ClassAccessors& GetAccessors();

using namespace Utility;

