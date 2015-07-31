// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainMaterial.h"
#include "../Assets/AssetServices.h"
#include "../Assets/InvalidAssetManager.h"

#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/FileUtils.h"

#include "../Utility/StringFormat.h"
#include "../Utility/Conversion.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/ParameterBox.h"

#include "../Utility/FunctionUtils.h"
#include "../ConsoleRig/Log.h"

namespace SceneEngine
{
///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Result, typename Type, typename PtrToMember>
        Result DefaultGetImp(const Type& type, PtrToMember ptrToMember) { return type.*ptrToMember; }

    template<typename ValueType, typename Type, typename PtrToMember>
        void DefaultSetImp(Type& type, PtrToMember ptrToMember, ValueType value) { type.*ptrToMember = value; }

    template<typename Type, typename Enable = void>
        struct MaybeRemoveRef;

    template<typename InType>
        struct MaybeRemoveRef<InType, typename std::enable_if<std::is_trivial<typename std::remove_reference<InType>::type>::value>::type>
        {
            using Type = typename std::remove_reference<InType>::type;
        };

    template<typename InType>
        struct MaybeRemoveRef<InType, typename std::enable_if<!std::is_trivial<typename std::remove_reference<InType>::type>::value>::type>
        {
            using Type = InType;
        };

    #define DefaultGet(InType, Member)                                                                              \
        [](const InType& t)                                                                                         \
        {                                                                                                           \
            using ResultType = MaybeRemoveRef<decltype(std::declval<const InType>().*(&InType::Member))>::Type;     \
            return DefaultGetImp<ResultType>(t, &InType::Member);                                                   \
        }                                                                                                           \
        /**/

    #define DefaultSet(InType, Member)                                                                                  \
        [](InType& t, MaybeRemoveRef<decltype(std::declval<const InType>().*(&InType::Member))>::Type value)            \
        {                                                                                                               \
            DefaultSetImp(t, &InType::Member,                                                                           \
                std::forward<MaybeRemoveRef<decltype(std::declval<const InType>().*(&InType::Member))>::Type>(value));  \
        }                                                                                                               \
        /**/

    template<typename Result, typename Type, typename PtrToMember>
        Result DefaultGetArrayImp(const Type& type, size_t arrayIndex, PtrToMember ptrToMember, size_t arrayCount)
        {
            if (arrayIndex > arrayCount) {
                assert(0);
                return Default<Result>();
            }
            return (type.*ptrToMember)[arrayIndex]; 
        }

    #define DefaultGetArray(InType, Member)                                                                             \
        [](const InType& t, size_t index)                                                                               \
        {                                                                                                               \
            using ArrayType = decltype(std::declval<const InType>().*(&InType::Member));                                \
            using ElementType = MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type;     \
            return DefaultGetArrayImp<ElementType>(t, index, &InType::Member, sizeof(ArrayType) / sizeof(ElementType)); \
        }                                                                                                               \
        /**/

    template<typename ValueType, typename Type, typename PtrToMember>
        void DefaultSetArrayImp(Type& type, size_t arrayIndex, PtrToMember ptrToMember, size_t arrayCount, ValueType value)
        {
            if (arrayIndex > arrayCount) {
                assert(0);
                return;
            }
            (type.*ptrToMember)[arrayIndex] = value;
        }

    #define DefaultSetArray(InType, Member)                                                                                             \
        [](InType& t, size_t index, MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type value)           \
        {                                                                                                                               \
            using ArrayType = decltype(std::declval<const InType>().*(&InType::Member));                                                \
            using ElementType = MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type;                     \
            return DefaultSetArrayImp<ElementType>(                                                                                     \
                t, index, &InType::Member, sizeof(ArrayType) / sizeof(ElementType),                                                     \
                std::forward<ElementType>(value));                                                                                      \
        }                                                                                                                               \
        /**/

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Obj, typename PtrToMember> struct PtrToMemberTarget 
    {
        using Type = decltype(std::declval<Obj>().*(std::declval<PtrToMember>()));
    };

    template<typename InType, typename PtrToMember>
        auto DefaultCreateImp(InType& t, PtrToMember ptrToMember)
            -> typename std::remove_reference<typename PtrToMemberTarget<InType, PtrToMember>::Type>::type::value_type&
        {
            auto& vec = (t.*ptrToMember);
            std::remove_reference<decltype(vec)>::type::value_type temp;
            vec.emplace_back(temp);
            return vec[vec.size()-1];
        }

    #define DefaultCreate(InType, Member)                               \
        [](void* t) -> void*                                            \
        {                                                               \
            return &DefaultCreateImp(*(InType*)t, &InType::Member);     \
        }                                                               \
        /**/

    template<typename InType, typename PtrToMember>
        auto DefaultGetChildByKeyImpl(const InType& t, PtrToMember ptrToMember, uint64 key)
            -> const typename std::remove_reference<typename PtrToMemberTarget<InType, PtrToMember>::Type>::type::value_type*
        {
            const auto& vec = (t.*ptrToMember);
            using ValueType = typename std::remove_reference<typename PtrToMemberTarget<InType, PtrToMember>::Type>::type::value_type;
            const auto& props = GetAccessors<ValueType>();
            static const auto KeyHash = Hash64("Key");
            for (const auto&i:vec) {
                uint64 ckey;
                if (props.TryGet<decltype(key)>(ckey, i, KeyHash))
                    if (ckey == key)
                        return &i;
            }
            return nullptr;
        }

    #define DefaultGetChildByKey(InType, Member)                                        \
        [](const void* t, uint64 key) -> const void*                                    \
        {                                                                               \
            return DefaultGetChildByKeyImpl(*(const InType*)t, &InType::Member, key);   \
        }                                                                               \
        /**/

    template<typename InType, typename PtrToMember>
        auto DefaultGetChildByIndexImpl(const InType& t, PtrToMember ptrToMember, size_t index)
            -> const typename std::remove_reference<typename PtrToMemberTarget<InType, PtrToMember>::Type>::type::value_type*
        {
            const auto& vec = (t.*ptrToMember);
            if (index < vec.size()) return &vec[index];
            return nullptr;
        }

    #define DefaultGetChildByIndex(InType, Member)                                          \
        [](const void* t, size_t index) -> const void*                                      \
        {                                                                                   \
            return DefaultGetChildByIndexImpl(*(const InType*)t, &InType::Member, index);   \
        }                                                                                   \
        /**/

    template<typename InType, typename PtrToMember>
        size_t DefaultGetCountImpl(const InType& t, PtrToMember ptrToMember)
        {
            auto& vec = (t.*ptrToMember);
            return vec.size();
        }

    #define DefaultGetCount(InType, Member)                                     \
        [](const void* t)                                                       \
        {                                                                       \
            return DefaultGetCountImpl(*(const InType*)t, &InType::Member);     \
        }                                                                       \
        /**/

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ClassAccessors;
    template<typename Type> const ClassAccessors& GetAccessors();

    template<typename Type> struct IsStringType { static const bool Result = false; };
    template<typename CharType, typename Allocator> struct IsStringType<std::basic_string<CharType, Allocator>> 
        { static const bool Result = true; };

    template<typename SetSig> struct SetterFnTraits { static const bool IsArrayForm = false; static const bool IsString = false; };
    template<typename ObjectTypeT, typename ValueTypeT> 
        struct SetterFnTraits<void(ObjectTypeT, ValueTypeT)>
        {
            using ValueType = ValueTypeT;
            using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
            static const bool IsArrayForm = false;
            static const bool IsString = IsStringType<std::remove_const<std::remove_reference<ValueTypeT>::type>::type>::Result;
        };

    template<typename ObjectTypeT, typename ValueTypeT> 
        struct SetterFnTraits<void(ObjectTypeT, size_t, ValueTypeT)>
        {
            using ValueType = ValueTypeT;
            using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
            static const bool IsArrayForm = true;
            static const bool IsString = IsStringType<std::remove_const<std::remove_reference<ValueTypeT>::type>::type>::Result;
        };

    template<typename SetSig> struct GetterFnTraits { static const bool IsArrayForm = false; static const bool IsString = false; };
    template<typename ObjectTypeT, typename ReturnTypeT> 
        struct GetterFnTraits<ReturnTypeT(ObjectTypeT)>
        {
            using ValueType = ReturnTypeT;
            using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
            static const bool IsArrayForm = false;
            static const bool IsString = IsStringType<std::remove_const<std::remove_reference<ReturnTypeT>::type>::type>::Result;
        };

    template<typename ObjectTypeT, typename ReturnTypeT> 
        struct GetterFnTraits<ReturnTypeT(ObjectTypeT, size_t)>
        {
            using ValueType = ReturnTypeT;
            using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
            static const bool IsArrayForm = true;
            static const bool IsString = IsStringType<std::remove_const<std::remove_reference<ReturnTypeT>::type>::type>::Result;
        };

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

            std::basic_string<utf8>     _name;
            CastFromFn                  _castFrom;
            CastFromArrayFn             _castFromArray;
            CastToFn                    _castTo;
            CastToArrayFn               _castToArray;
            size_t                      _fixedArrayLength;
        };

        class ChildList
        {
        public:
            using CreateFn      = std::function<void*(void*)>;
            using GetCountFn    = std::function<size_t(const void*)>;
            using GetByIndexFn  = std::function<const void*(const void*,size_t)>;
            using GetByKeyFn    = std::function<const void*(const void*,uint64)>;

            std::basic_string<utf8>     _name;
            const ClassAccessors*   _childProps;
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

        bool TryCastFrom(
            void* dst, uint64 id,
            const void* src, ImpliedTyping::TypeDesc srcType,
            bool stringForm = false) const;

        bool TryCastFrom(
            void* dst, uint64 id, size_t arrayIndex,
            const void* src, ImpliedTyping::TypeDesc srcType,
            bool stringForm = false) const;

        template<typename ResultType, typename Type>
            bool TryGet(ResultType& result, const Type& src, uint64 id) const;

        template<typename Type>
            bool TryCastFrom(
                Type& dst, uint64 id,
                const void* src, ImpliedTyping::TypeDesc srcType,
                bool stringForm = false) const;
            
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
                ////   S E T T E R   I N T E R F A C E   ////

        std::pair<void*, const ClassAccessors*> TryCreateChild(void* dst, uint64 childListId) const;

        template<typename Type>
            std::pair<void*, const ClassAccessors*> TryCreateChild(Type& dst, uint64 childListId) const;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
                ////   I N I T I A L I Z A T I O N   I N T E R F A C E   ////

        template<typename GetFn, typename SetFn>
            void Add(const utf8 name[], GetFn&& getter, SetFn&& setter, size_t fixedArrayLength = 1);

        template<typename ChildType, typename CreateFn, typename GetCountFn, typename GetByIndexFn, typename GetByKeyFn>
            void AddChildList(const utf8 name[], CreateFn&&, GetCountFn&&, GetByIndexFn&&, GetByKeyFn&&);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ClassAccessors(size_t associatedType) : _associatedType(associatedType) {}
        ~ClassAccessors();

    protected:
        size_t _associatedType;

        template<typename SetSig, typename std::enable_if<!SetterFnTraits<SetSig>::IsArrayForm>::type* = nullptr>
            void MaybeAddCasterForSet(uint64 id, std::function<SetSig>&& setter);

        template<typename SetSig, typename std::enable_if<SetterFnTraits<SetSig>::IsArrayForm>::type* = nullptr>
            void MaybeAddCasterForSet(uint64 id, std::function<SetSig>&& setter);

        template<typename GetSig, typename std::enable_if<!GetterFnTraits<GetSig>::IsArrayForm>::type* = nullptr>
            void MaybeAddCasterForGet(uint64 id, std::function<GetSig>&& getter);

        template<typename GetSig, typename std::enable_if<GetterFnTraits<GetSig>::IsArrayForm>::type* = nullptr>
            void MaybeAddCasterForGet(uint64 id, std::function<GetSig>&& getter);

        Property& PropertyForId(uint64 id);

        VariantFunctions _getters;
        VariantFunctions _setters;
        std::vector<std::pair<uint64, Property>> _properties;
        std::vector<std::pair<uint64, ChildList>> _childLists;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    static const auto ParseBufferSize = 256u;

    template<typename SetFn, typename std::enable_if<!SetterFnTraits<SetFn>::IsString>::type* = nullptr>
        bool DefaultCastFrom(
            void* obj, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
            std::function<SetFn> setFn)
        {
            static_assert(SetterFnTraits<SetFn>::IsArrayForm == false, "Mismatch on setter array form");
            using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
            using Type = SetterFnTraits<SetFn>::ObjectType;

            char buffer[ParseBufferSize];
            auto destType = ImpliedTyping::TypeOf<PassToSetter>();
            if (srcStringForm) {
                char buffer2[ParseBufferSize];
                auto parsedType = ImpliedTyping::Parse(
                    (const char*)src, (const char*)PtrAdd(src, srcType.GetSize()),
                    buffer2, sizeof(buffer2));
                if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                if (!ImpliedTyping::Cast(
                    buffer, sizeof(buffer), destType, 
                    buffer2, parsedType))
                    return false;
            } else {
                if (!ImpliedTyping::Cast(
                    buffer, sizeof(buffer), destType, 
                    src, srcType))
                    return false;
            }
            
            setFn(*(std::remove_reference<Type>::type*)obj, *(const PassToSetter*)buffer);
            return true;
        }

    template<typename SetFn, typename std::enable_if<SetterFnTraits<SetFn>::IsString>::type* = nullptr>
        bool DefaultCastFrom(
            void* obj, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
            std::function<SetFn> setFn)
        {
            static_assert(SetterFnTraits<SetFn>::IsArrayForm == false, "Mismatch on setter array form");
            using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
            using Type = SetterFnTraits<SetFn>::ObjectType;

            using DestCharType = PassToSetter::value_type;
            if (srcStringForm) {
                setFn(
                    *(typename std::remove_reference<Type>::type*)obj,
                    std::basic_string<DestCharType>(
                        (const DestCharType*)src,
                        (const DestCharType*)PtrAdd(src,srcType.GetSize())));
            } else {
                setFn(
                    *(typename std::remove_reference<Type>::type*)obj,
                    ImpliedTyping::AsString(src, srcType.GetSize(), srcType, true));
            }
            return true;
        }

    template<typename SetFn, typename std::enable_if<!SetterFnTraits<SetFn>::IsString>::type* = nullptr>
        bool DefaultArrayCastFrom(
            void* obj, size_t arrayIndex, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
            std::function<SetFn> setFn)
        {
            static_assert(SetterFnTraits<SetFn>::IsArrayForm == true, "Mismatch on setter array form");
            using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
            using Type = SetterFnTraits<SetFn>::ObjectType;

            char buffer[ParseBufferSize];
            auto destType = ImpliedTyping::TypeOf<PassToSetter>();
            if (srcStringForm) {
                char buffer2[ParseBufferSize];
                auto parsedType = ImpliedTyping::Parse(
                    (const char*)src, (const char*)PtrAdd(src, srcType.GetSize()),
                    buffer2, sizeof(buffer2));
                if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                if (!ImpliedTyping::Cast(
                    buffer, sizeof(buffer), destType, 
                    buffer2, parsedType))
                    return false;
            } else {
                if (!ImpliedTyping::Cast(
                    buffer, sizeof(buffer), destType, 
                    src, srcType))
                    return false;
            }
                
            setFn(*(typename std::remove_reference<Type>::type*)obj, arrayIndex, *(const PassToSetter*)buffer);
            return true;
        }


    template<typename SetFn, typename std::enable_if<SetterFnTraits<SetFn>::IsString>::type* = nullptr>
        bool DefaultArrayCastFrom(
            void* obj, size_t arrayIndex, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
            std::function<SetFn> setFn)
        {
            static_assert(SetterFnTraits<SetFn>::IsArrayForm == true, "Mismatch on setter array form");
            using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
            using Type = SetterFnTraits<SetFn>::ObjectType;

            using DestCharType = PassToSetter::value_type;
            if (srcStringForm) {
                setFn(
                    *(typename std::remove_reference<Type>::type*)obj, arrayIndex, 
                    std::basic_string<DestCharType>(
                        (const DestCharType*)src,
                        (const DestCharType*)PtrAdd(src,srcType.GetSize())));
            } else {
                setFn(
                    *(typename std::remove_reference<Type>::type*)obj, arrayIndex, 
                    ImpliedTyping::AsString(src, srcType.GetSize(), srcType, true));
            }

            return true;
        }

    template<typename SrcType, typename std::enable_if<!IsStringType<SrcType>::Result>::type* = nullptr>
        static bool CastToHelper(
            void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType, bool dstStringForm,
            const SrcType& src)
    {
        if (dstStringForm) {
            XlCopyString((char*)dst, dstSize / sizeof(char), ImpliedTyping::AsString(src, true).c_str());
            return true;
        } else {
            return ImpliedTyping::Cast(dst, dstSize, dstType, &src, ImpliedTyping::TypeOf<SrcType>());
        }
    }

    template<typename SrcType, typename std::enable_if<IsStringType<SrcType>::Result>::type* = nullptr>
        static bool CastToHelper(
            void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType, bool dstStringForm,
            const SrcType& src)
    {
        if (dstStringForm) {
            XlCopyString((char*)dst, dstSize / sizeof(char), src.c_str());
            return true;
        } else {
            char parseBuffer[ParseBufferSize];
            auto parseType = ImpliedTyping::Parse(
                AsPointer(src.cbegin()), AsPointer(src.cend()),
                parseBuffer, sizeof(parseBuffer));
            if (parseType._type == ImpliedTyping::TypeCat::Void) return false;
            return ImpliedTyping::Cast(dst, dstSize, dstType, parseBuffer, parseType);
        }
    }

    template<typename GetFn>
        bool DefaultCastTo(
            const void* obj, void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType, bool dstStringForm,
            std::function<GetFn> getFn)
        {
            auto temp = getFn(*(const typename GetterFnTraits<GetFn>::ObjectType*)obj);
            return CastToHelper(dst, dstSize, dstType, dstStringForm, temp);
        }

    template<typename GetFn>
        bool DefaultArrayCastTo(
            const void* obj, size_t arrayIndex, void* dst, size_t dstSize, ImpliedTyping::TypeDesc dstType, bool dstStringForm,
            std::function<GetFn> getFn)
        {
            auto temp = getFn(*(const typename GetterFnTraits<GetFn>::ObjectType*)obj, arrayIndex);
            return CastToHelper(dst, dstSize, dstType, dstStringForm, temp);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Fn> struct ExtractFunctionSig;
    template<typename FnSig> 
        struct ExtractFunctionSig<std::function<FnSig>>
    {
        using Type = FnSig;
    };

    template<typename GetFn, typename SetFn>
        void ClassAccessors::Add(
            const utf8 name[],
            GetFn&& getter,
            SetFn&& setter,
            size_t fixedArrayLength)
        {
            auto g = MakeFunction(std::move(getter));
            auto s = MakeFunction(std::move(setter));
            auto scopy = s;
            auto gcopy = g;
            auto id = Hash64((const char*)name);
            _getters.Add(id, std::move(g));
            _setters.Add(id, std::move(s));

            auto& p = PropertyForId(id);
            p._name = name;
            p._fixedArrayLength = fixedArrayLength;

                // Generate casting functions
                // Casting functions always have the same signature
                //      --  this makes it easier to iterate through them
                //          during serialization (etc)
            
            MaybeAddCasterForSet(id, std::move(scopy));
            MaybeAddCasterForGet(id, std::move(gcopy));
        }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    template<typename SetSig, typename std::enable_if<!SetterFnTraits<SetSig>::IsArrayForm>::type*>
        void ClassAccessors::MaybeAddCasterForSet(
            uint64 id, std::function<SetSig>&& setter)
        {
            using namespace std::placeholders;
            PropertyForId(id)._castFrom = std::bind(&DefaultCastFrom<SetSig>, _1, _2, _3, _4, std::move(setter));
        }

    template<typename SetSig, typename std::enable_if<SetterFnTraits<SetSig>::IsArrayForm>::type*>
        void ClassAccessors::MaybeAddCasterForSet(
            uint64 id, std::function<SetSig>&& setter)
        {
            using namespace std::placeholders;
            PropertyForId(id)._castFromArray = std::bind(&DefaultArrayCastFrom<SetSig>, _1, _2, _3, _4, _5, std::move(setter));
        }
    
    template<>
        void ClassAccessors::MaybeAddCasterForSet(
            uint64 id, std::function<void()>&& setter) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto ClassAccessors::PropertyForId(uint64 id) -> Property&
    {
        auto i = LowerBound(_properties, id);
        if (i==_properties.end() || i->first != id)
            i=_properties.insert(i, std::make_pair(id, Property()));
        return i->second;
    }

    template<typename GetSig, typename std::enable_if<!GetterFnTraits<GetSig>::IsArrayForm>::type*>
        void ClassAccessors::MaybeAddCasterForGet(
            uint64 id, std::function<GetSig>&& getter)
        {
            using namespace std::placeholders;
            PropertyForId(id)._castTo = std::bind(&DefaultCastTo<GetSig>, _1, _2, _3, _4, _5, std::move(getter));
        }

    template<typename GetSig, typename std::enable_if<GetterFnTraits<GetSig>::IsArrayForm>::type*>
        void ClassAccessors::MaybeAddCasterForGet(
            uint64 id, std::function<GetSig>&& getter)
        {
            using namespace std::placeholders;
            PropertyForId(id)._castToArray = std::bind(&DefaultArrayCastTo<GetSig>, _1, _2, _3, _4, _5, _6, std::move(getter));
        }
    
    template<>
        void ClassAccessors::MaybeAddCasterForGet(
            uint64 id, std::function<void()>&& getter) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename ChildType, typename CreateFn, typename GetCountFn, typename GetByIndexFn, typename GetByKeyFn>
        void ClassAccessors::AddChildList(
            const utf8 name[], 
            CreateFn&& createFn, GetCountFn&& getCountFn, GetByIndexFn&& getByIndexFn, GetByKeyFn&& getByKeyFn)
        {
            auto id = Hash64((const char*)name);
            auto c = MakeFunction(std::move(createFn));
            auto gc = MakeFunction(std::move(getCountFn));
            auto gi = MakeFunction(std::move(getByIndexFn));
            auto gk = MakeFunction(std::move(getByKeyFn));

            ChildList child;
            child._name = name;
            child._childProps = &GetAccessors<ChildType>();
            child._createFn = std::move(c);
            child._getCount = std::move(gc);
            child._getByIndex = std::move(gi);
            child._getByKeyFn = std::move(gk);
            auto i = LowerBound(_childLists, id);
            _childLists.insert(i, std::make_pair(id, std::move(child)));
        }

    bool ClassAccessors::TryCastFrom(
        void* dst,
        uint64 id,
        const void* src,
        ImpliedTyping::TypeDesc srcType,
        bool stringForm) const
    {
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id) {
            if (i->second._castFrom)
                return i->second._castFrom(dst, src, srcType, stringForm);

            if (i->second._castFromArray) {
                    // If there is an array form, then we can try to
                    // set all of the members of the array at the same time
                    // First, we'll use the implied typing system to break down
                    // our input into array components.. Then we'll set each
                    // element individually.
                char buffer[ParseBufferSize];
                if (stringForm) {
                    auto parsedType = ImpliedTyping::Parse(
                        (const char*)src, (const char*)PtrAdd(src, srcType.GetSize()),
                        buffer, sizeof(buffer));
                    if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                    srcType = parsedType;
                    src = buffer;
                }

                bool result = false;
                auto elementDesc = ImpliedTyping::TypeDesc(srcType._type);
                auto elementSize = ImpliedTyping::TypeDesc(srcType._type).GetSize();
                for (unsigned c=0; c<srcType._arrayCount; ++c) {
                    auto* e = PtrAdd(src, c*elementSize);
                    result |= i->second._castFromArray(dst, c, e, elementDesc, false);
                }
                return result;
            }
        }

        return false;
    }

    bool ClassAccessors::TryCastFrom(
        void* dst,
        uint64 id, size_t arrayIndex,
        const void* src,
        ImpliedTyping::TypeDesc srcType,
        bool stringForm) const
    {
        auto i = LowerBound(_properties, id);
        if (i!=_properties.end() && i->first == id)
            return i->second._castFromArray(dst, arrayIndex, src, srcType, stringForm);
        return false;
    }

    std::pair<void*, const ClassAccessors*> ClassAccessors::TryCreateChild(
        void* dst, uint64 childListId) const
    {
        auto i = LowerBound(_childLists, childListId);
        if (i!=_childLists.end() && i->first == childListId) {
            void* created = i->second._createFn(dst);
            return std::make_pair(created, i->second._childProps);
        }
        return std::make_pair(nullptr, nullptr);
    }

    template<typename Type>
        bool ClassAccessors::TryCastFrom(
            Type& dst,
            uint64 id, const void* src,
            ImpliedTyping::TypeDesc srcType, bool stringForm) const
        {
            assert(typeid(Type).hash_code() == _associatedType);
            return TryCastFrom(&dst, id, src, srcType, stringForm);
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

    ClassAccessors::~ClassAccessors() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<> const ClassAccessors& GetAccessors<TerrainMaterialConfig>()
    {
        using Obj = TerrainMaterialConfig;
        static ClassAccessors props(typeid(Obj).hash_code());
        static bool init = false;
        if (!init) {
            props.Add(
                u("DiffuseDims"), 
                DefaultGet(Obj, _diffuseDims),
                DefaultSet(Obj, _diffuseDims));
            props.Add(
                u("NormalDims"), 
                DefaultGet(Obj, _normalDims),
                DefaultSet(Obj, _normalDims));
            props.Add(
                u("ParamDims"), 
                DefaultGet(Obj, _paramDims),
                DefaultSet(Obj, _paramDims));

            props.AddChildList<Obj::GradFlagMaterial>(
                u("GradFlagMaterial"),
                DefaultCreate(Obj, _gradFlagMaterials),
                DefaultGetCount(Obj, _gradFlagMaterials),
                DefaultGetChildByIndex(Obj, _gradFlagMaterials),
                DefaultGetChildByKey(Obj, _gradFlagMaterials));

            props.AddChildList<TerrainMaterialConfig::ProcTextureSetting>(
                u("ProcTextureSetting"),
                DefaultCreate(Obj, _procTextures),
                DefaultGetCount(Obj, _procTextures),
                DefaultGetChildByIndex(Obj, _procTextures),
                DefaultGetChildByKey(Obj, _procTextures));

            init = true;
        }
        return props;
    }

    template<> const ClassAccessors& GetAccessors<TerrainMaterialConfig::GradFlagMaterial>()
    {
        using Obj = TerrainMaterialConfig::GradFlagMaterial;
        static ClassAccessors props(typeid(Obj).hash_code());
        static bool init = false;
        if (!init) {
            props.Add(
                u("MaterialId"), 
                DefaultGet(Obj, _id),
                DefaultSet(Obj, _id));
            props.Add(
                u("Texture"),
                DefaultGetArray(Obj, _texture),
                DefaultSetArray(Obj, _texture),
                dimof(std::declval<Obj>()._texture));
            props.Add(
                u("Mapping"), 
                DefaultGetArray(Obj, _mappingConstant),
                DefaultSetArray(Obj, _mappingConstant),
                dimof(std::declval<Obj>()._texture));
            props.Add(
                u("Key"), 
                [](const Obj& mat) { return mat._id; },
                nullptr);

            init = true;
        }
        return props;
    }

    template<> const ClassAccessors& GetAccessors<TerrainMaterialConfig::ProcTextureSetting>()
    {
        using Obj = TerrainMaterialConfig::ProcTextureSetting;
        static ClassAccessors props(typeid(Obj).hash_code());
        static bool init = false;
        if (!init) {
            props.Add(
                u("Name"), 
                DefaultGet(Obj, _name),
                DefaultSet(Obj, _name));
            props.Add(
                u("Texture"),
                DefaultGetArray(Obj, _texture),
                DefaultSetArray(Obj, _texture),
                dimof(std::declval<Obj>()._texture));
            props.Add(
                u("HGrid"), 
                DefaultGet(Obj, _hgrid),
                DefaultSet(Obj, _hgrid));
            props.Add(
                u("Gain"), 
                DefaultGet(Obj, _gain),
                DefaultSet(Obj, _gain));
            init = true;
        }
        return props;
    }

}

namespace SceneEngine
{

    template<typename Formatter>
        void AccessorDeserialize(
            Formatter& formatter,
            void* obj, const ClassAccessors& props)
    {
        using Blob = Formatter::Blob;
        using CharType = Formatter::value_type;
        auto charTypeCat = ImpliedTyping::TypeOf<CharType>()._type;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    typename Formatter::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in begin element", formatter.GetLocation()));
                    
                    auto arrayBracket = std::find(name._start, name._end, '[');
                    if (arrayBracket == name._end) {
                        if (!props.TryCastFrom(
                            obj,
                            Hash64(name._start, name._end), value._start, 
                            ImpliedTyping::TypeDesc(charTypeCat, uint16(value._end - value._start)), true)) {

                            LogWarning << "Failure while assigning property during deserialization -- " << 
                                Conversion::Convert<std::string>(std::basic_string<CharType>(name._start, name._end));
                        }
                    } else {
                        auto arrayIndex = XlAtoUI32((const char*)(arrayBracket+1));
                        if (!props.TryCastFrom(
                            obj, Hash64(name._start, arrayBracket), arrayIndex, value._start, 
                            ImpliedTyping::TypeDesc(charTypeCat, uint16(value._end - value._start)), true)) {

                            LogWarning << "Failure while assigning array property during deserialization -- " << 
                                Conversion::Convert<std::string>(std::basic_string<CharType>(name._start, name._end));
                        }
                    }
                }
                break;

            case Blob::EndElement:
            case Blob::None:
                return;

            case Blob::BeginElement:
                {
                    typename Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName))
                        Throw(FormatException("Error in begin element", formatter.GetLocation()));

                    auto created = props.TryCreateChild(obj, Hash64(eleName._start, eleName._end));
                    if (created.first) {
                        AccessorDeserialize(formatter, created.first, *created.second);
                    } else {
                        LogWarning << "Couldn't find a match for element name during deserialization -- " << 
                            Conversion::Convert<std::string>(std::basic_string<CharType>(eleName._start, eleName._end));
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    break;
                }
            }
        }
    }

    template<typename Formatter, typename Type>
        void AccessorDeserialize(
            Formatter& formatter,
            Type& obj)
        {
            const auto& props = GetAccessors<Type>();
            AccessorDeserialize(formatter, &obj, props);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void AccessorSerialize(
        OutputStreamFormatter& formatter,
        const void* obj, const ClassAccessors& props)
    {
        using CharType = utf8;
        auto charTypeCat = ImpliedTyping::TypeOf<CharType>()._type;
        CharType buffer[ParseBufferSize];

        for (size_t i=0; i<props.GetPropertyCount(); ++i) {
            const auto& p = props.GetPropertyByIndex(i);
            if (p._castTo) {
                p._castTo(
                    obj, buffer, sizeof(buffer), 
                    ImpliedTyping::TypeDesc(charTypeCat, dimof(buffer)), true);

                formatter.WriteAttribute(
                    AsPointer(p._name.cbegin()), AsPointer(p._name.cend()), 
                    buffer, &buffer[XlStringLen(buffer)]);
            }

            if (p._castToArray) {
                for (size_t e=0; e<p._fixedArrayLength; ++e) {
                    p._castToArray(
                        obj, e, buffer, sizeof(buffer), 
                        ImpliedTyping::TypeDesc(charTypeCat, dimof(buffer)), true);

                    StringMeld<256, CharType> name;
                    name << p._name.c_str() << "[" << e << "]";
                    formatter.WriteAttribute(name.get(), buffer);
                }
            }
        }

        for (size_t i=0; i<props.GetChildListCount(); ++i) {
            const auto& childList = props.GetChildListByIndex(i);
            auto count = childList._getCount(obj);
            for (size_t e=0; e<count; ++e) {
                const auto* child = childList._getByIndex(obj, e);
                auto eleId = formatter.BeginElement(childList._name);
                AccessorSerialize(formatter, child, *childList._childProps);
                formatter.EndElement(eleId);
            }
        }
    }

    template<typename Type>
        void AccessorSerialize(
            OutputStreamFormatter& formatter,
            const Type& obj)
        {
            const auto& props = GetAccessors<Type>();
            AccessorSerialize(formatter, &obj, props);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void TerrainMaterialConfig::Write(OutputStreamFormatter& formatter) const
    {
        AccessorSerialize(formatter, *this);
    }

    TerrainMaterialConfig::TerrainMaterialConfig()
    {
        _diffuseDims = _normalDims = _paramDims = UInt2(32, 32);
    }

    TerrainMaterialConfig::TerrainMaterialConfig(
        InputStreamFormatter<utf8>& formatter,
        const ::Assets::DirectorySearchRules& searchRules)
    : TerrainMaterialConfig()
    {
        AccessorDeserialize(formatter, *this);
    }

    TerrainMaterialConfig::~TerrainMaterialConfig() {}

///////////////////////////////////////////////////////////////////////////////////////////////////
        // Previous implementations (for performance comparisons)...

    template<typename InputType>
        ::Assets::rstring AsRString(InputType input) { return Conversion::Convert<::Assets::rstring>(input); }
        
    static const utf8* TextureNames[] = { u("Texture0"), u("Texture1"), u("Slopes") };

    TerrainMaterialConfig::TerrainMaterialConfig(
        InputStreamFormatter<utf8>& formatter,
        const ::Assets::DirectorySearchRules& searchRules,
        bool)
    : TerrainMaterialConfig()
    {
        Document<InputStreamFormatter<utf8>> doc(formatter);

        for (auto matCfg=doc.FirstChild(); matCfg; matCfg=matCfg.NextSibling()) {
            if (XlEqString(matCfg.Name(), u("StrataMaterial"))) {

                StrataMaterial mat;
                mat._id = Deserialize(matCfg, u("MaterialId"), 0u);

                auto strata = matCfg.Element(u("Strata"));
                unsigned strataCount = 0;
                for (auto c = strata.FirstChild(); c; c = c.NextSibling()) { ++strataCount; }

                unsigned strataIndex = 0;
                for (auto c = strata.FirstChild(); c; c = c.NextSibling(), ++strataIndex) {
                    StrataMaterial::Strata newStrata;
                    for (unsigned t=0; t<dimof(TextureNames); ++t) {
                        auto tName = c.Attribute(TextureNames[t]).Value();
                        if (XlCompareStringI(tName.c_str(), u("null"))!=0)
                            newStrata._texture[t] = Conversion::Convert<::Assets::rstring>(tName);
                    }

                    newStrata._endHeight = Deserialize(c, u("EndHeight"), 0.f);
                    auto mappingConst = Deserialize(c, u("Mapping"), Float4(1.f, 1.f, 1.f, 1.f));
                    newStrata._mappingConstant[0] = mappingConst[0];
                    newStrata._mappingConstant[1] = mappingConst[1];
                    newStrata._mappingConstant[2] = mappingConst[2];

                    mat._strata.push_back(newStrata);
                }

                _strataMaterials.push_back(std::move(mat));

            } else if (XlEqString(matCfg.Name(), u("GradFlagMaterial"))) {

                GradFlagMaterial mat;
                mat._id = Deserialize(matCfg, u("MaterialId"), 0);
            
                mat._texture[0] = AsRString(matCfg.Attribute(u("Texture0")).Value());
                mat._texture[1] = AsRString(matCfg.Attribute(u("Texture1")).Value());
                mat._texture[2] = AsRString(matCfg.Attribute(u("Texture2")).Value());
                mat._texture[3] = AsRString(matCfg.Attribute(u("Texture3")).Value());
                mat._texture[4] = AsRString(matCfg.Attribute(u("Texture4")).Value());

                char buffer[512];
                auto mappingAttr = matCfg.Attribute(u("Mapping")).Value();
                auto parsedType = ImpliedTyping::Parse(
                    (const char*)AsPointer(mappingAttr.cbegin()), (const char*)AsPointer(mappingAttr.cend()),
                    buffer, sizeof(buffer));
                ImpliedTyping::Cast(
                    mat._mappingConstant, sizeof(mat._mappingConstant), 
                    ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, dimof(mat._mappingConstant)),
                    buffer, parsedType);
                
                _gradFlagMaterials.push_back(mat);

            } else if (XlEqString(matCfg.Name(), u("GradFlagMaterial"))) {

                ProcTextureSetting mat;
                mat._name = AsRString(matCfg.Attribute(u("Name")).Value());
                mat._texture[0] = AsRString(matCfg.Attribute(u("Texture0")).Value());
                mat._texture[1] = AsRString(matCfg.Attribute(u("Texture1")).Value());
                mat._hgrid = Deserialize(matCfg, u("HGrid"), mat._hgrid);
                mat._gain = Deserialize(matCfg, u("Gain"), mat._gain);
                _procTextures.push_back(mat);

            }
        }

        _searchRules = searchRules;
    }

    #if 0
        Serialize(formatter, u("DiffuseDims"), _diffuseDims);
        Serialize(formatter, u("NormalDims"), _normalDims);
        Serialize(formatter, u("ParamDims"), _paramDims);

        for (auto mat=_strataMaterials.cbegin(); mat!=_strataMaterials.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("StrataMaterial"));
            Serialize(formatter, "MaterialId", mat->_id);

            auto strataList = formatter.BeginElement(u("Strata"));
            unsigned strataIndex = 0;
            for (auto s=mat->_strata.cbegin(); s!=mat->_strata.cend(); ++s, ++strataIndex) {
                auto strata = formatter.BeginElement((StringMeld<64, utf8>() << "Strata" << strataIndex).get());
                for (unsigned t=0; t<dimof(TextureNames); ++t)
                    formatter.WriteAttribute(TextureNames[t], Conversion::Convert<std::basic_string<utf8>>(s->_texture[t]));

                Serialize(formatter, "EndHeight", s->_endHeight);
                Serialize(formatter, "Mapping", Float4(s->_mappingConstant[0], s->_mappingConstant[1], s->_mappingConstant[2], 1.f));
                formatter.EndElement(strata);
            }
            formatter.EndElement(strataList);
            formatter.EndElement(matEle);
        }

        for (auto mat=_gradFlagMaterials.cbegin(); mat!=_gradFlagMaterials.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("GradFlagMaterial"));
            Serialize(formatter, "MaterialId", mat->_id);
            
            Serialize(formatter, "Texture0", mat->_texture[0]);
            Serialize(formatter, "Texture1", mat->_texture[1]);
            Serialize(formatter, "Texture2", mat->_texture[2]);
            Serialize(formatter, "Texture3", mat->_texture[3]);
            Serialize(formatter, "Texture4", mat->_texture[4]);
            using namespace ImpliedTyping;
            Serialize(formatter, "Mapping", 
                AsString(
                    mat->_mappingConstant, sizeof(mat->_mappingConstant),
                    TypeDesc(TypeCat::Float, dimof(mat->_mappingConstant)), true));
            formatter.EndElement(matEle);
        }

        for (auto mat=_procTextures.cbegin(); mat!=_procTextures.cend(); ++mat) {
            auto matEle = formatter.BeginElement(u("ProcTextureSetting"));
            Serialize(formatter, "Name", mat->_name);
            Serialize(formatter, "Texture0", mat->_texture[0]);
            Serialize(formatter, "Texture1", mat->_texture[1]);
            Serialize(formatter, "HGrid", mat->_hgrid);
            Serialize(formatter, "Gain", mat->_gain);
            formatter.EndElement(matEle);
        }
    #endif

}

