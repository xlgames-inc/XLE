// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ClassAccessors.h"
#include "../FunctionUtils.h"
#include "../ParameterBox.h"
#include "../StringUtils.h"
#include "../MemoryUtils.h" // for Hash64

    //  This file contains functions that are useful when implementing GetAccessors<>.
    //  However, to use the ClassAccessors interface, just include "ClassAccessors.h"

namespace Utility
{
    namespace Internal
    {
        static const auto ParseBufferSize = 256u;

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   D E F A U L T   G E T   &   S E T

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

		template<typename Type, typename std::enable_if<!std::is_lvalue_reference<Type>::value>::type* = nullptr>
			Type DefaultWithStaticRef() { return Default<Type>(); }

		template<typename Type, typename std::enable_if<std::is_lvalue_reference<Type>::value>::type* = nullptr>
			Type DefaultWithStaticRef() 
			{ 
				using NoRef = typename std::remove_reference<Type>::type;
				static NoRef s_retainedDefault = Default<NoRef>();
				return s_retainedDefault;
			}

		#define DefaultGet(InType, Member)                                                                              \
            [](const InType& t)                                                                                         \
            {                                                                                                           \
                using namespace Utility::Internal;                                                                      \
				using ResultType = MaybeRemoveRef<const decltype(InType::Member)&>::Type;								\
                return DefaultGetImp<ResultType>(t, &InType::Member);                                                   \
            }                                                                                                           \
            /**/

        #define DefaultSet(InType, Member)                                                                              \
            [](InType& t, Utility::Internal::MaybeRemoveRef<const decltype(InType::Member)&>::Type value)				\
            {                                                                                                           \
                using namespace Utility::Internal;                                                                      \
				using PassType = MaybeRemoveRef<const decltype(InType::Member)&>::Type;									\
                DefaultSetImp(t, &InType::Member, std::forward<PassType>(value));										\
            }                                                                                                           \
            /**/

        template<typename Result, typename Type, typename PtrToMember>
            Result DefaultGetArrayImp(const Type& type, size_t arrayIndex, PtrToMember ptrToMember, size_t arrayCount)
            {
                if (arrayIndex > arrayCount) {
                    assert(0);
                    return DefaultWithStaticRef<Result>();
                }
                return (type.*ptrToMember)[arrayIndex]; 
            }

        #define DefaultGetArray(InType, Member, Idx)                                                                        \
            [](const InType& t)																								\
            {                                                                                                               \
                using namespace Utility::Internal;                                                                          \
                using ArrayType = decltype(std::declval<const InType>().*(&InType::Member));                                \
                using ElementType = MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type;     \
                return DefaultGetArrayImp<ElementType>(t, Idx, &InType::Member, sizeof(ArrayType) / sizeof(ElementType));	\
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

        #define DefaultSetArray(InType, Member, Idx)                                                                                        \
            [](InType& t, Utility::Internal::MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type value)           \
            {                                                                                                                               \
                using namespace Utility::Internal;                                                                                          \
                using ArrayType = decltype(std::declval<const InType>().*(&InType::Member));                                                \
                using ElementType = MaybeRemoveRef<decltype(*(std::declval<const InType>().*(&InType::Member)))>::Type;                     \
                return DefaultSetArrayImp<ElementType>(                                                                                     \
                    t, Idx, &InType::Member, sizeof(ArrayType) / sizeof(ElementType),														\
                    std::forward<ElementType>(value));                                                                                      \
            }                                                                                                                               \
            /**/

////////////////////////////////////////////////////////////////////////////////////////////////////

        template<typename Obj, typename PtrToMember> struct PtrToMemberTarget 
        {
            using Type = decltype(std::declval<Obj>().*(std::declval<PtrToMember>()));
        };

        template<typename InType, typename PtrToMember>
            auto DefaultCreateImp(InType& t, PtrToMember ptrToMember)
                -> typename std::remove_reference<typename PtrToMemberTarget<InType, PtrToMember>::Type>::type::value_type&
            {
                auto& vec = (t.*ptrToMember);
                typename std::remove_reference<decltype(vec)>::type::value_type temp;
                vec.emplace_back(temp);
                return vec[vec.size()-1];
            }

        #define DefaultCreate(InType, Member)                               \
            [](void* t) -> void*                                            \
            {                                                               \
                using namespace Utility::Internal;                          \
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
                    if (props.template TryGet<decltype(key)>(ckey, i, KeyHash))
                        if (ckey == key)
                            return &i;
                }
                return nullptr;
            }

        #define DefaultGetChildByKey(InType, Member)                                        \
            [](const void* t, uint64 key) -> const void*                                    \
            {                                                                               \
                using namespace Utility::Internal;                                          \
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
                using namespace Utility::Internal;                                              \
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
                using namespace Utility::Internal;                                  \
                return DefaultGetCountImpl(*(const InType*)t, &InType::Member);     \
            }                                                                       \
            /**/

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   T R A I T S

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
                static const bool IsString = IsStringType<typename std::remove_const<typename std::remove_reference<ValueTypeT>::type>::type>::Result;
            };

        template<typename ObjectTypeT, typename ValueTypeT> 
            struct SetterFnTraits<void(ObjectTypeT, size_t, ValueTypeT)>
            {
                using ValueType = ValueTypeT;
                using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
                static const bool IsArrayForm = true;
                static const bool IsString = IsStringType<typename std::remove_const<typename std::remove_reference<ValueTypeT>::type>::type>::Result;
            };

        template<typename SetSig> struct GetterFnTraits { static const bool IsArrayForm = false; static const bool IsString = false; };
        template<typename ObjectTypeT, typename ReturnTypeT> 
            struct GetterFnTraits<ReturnTypeT(ObjectTypeT)>
            {
                using ValueType = ReturnTypeT;
                using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
                static const bool IsArrayForm = false;
                static const bool IsString = IsStringType<typename std::remove_const<typename std::remove_reference<ReturnTypeT>::type>::type>::Result;
            };

        template<typename ObjectTypeT, typename ReturnTypeT> 
            struct GetterFnTraits<ReturnTypeT(ObjectTypeT, size_t)>
            {
                using ValueType = ReturnTypeT;
                using ObjectType = typename std::remove_reference<ObjectTypeT>::type;
                static const bool IsArrayForm = true;
                static const bool IsString = IsStringType<typename std::remove_const<typename std::remove_reference<ReturnTypeT>::type>::type>::Result;
            };

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   C A S T I N G   O P E R A T O R S

        template<typename SetFn, typename std::enable_if<!SetterFnTraits<SetFn>::IsString>::type* = nullptr>
            bool DefaultCastFrom(
                void* obj, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
                std::function<SetFn> setFn)
            {
                static_assert(SetterFnTraits<SetFn>::IsArrayForm == false, "Mismatch on setter array form");
                using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
                using Type = typename SetterFnTraits<SetFn>::ObjectType;

                char buffer[ParseBufferSize];
                auto destType = ImpliedTyping::TypeOf<PassToSetter>();
                if (srcStringForm) {
                    char buffer2[ParseBufferSize];
                    auto parsedType = ImpliedTyping::Parse(
                        MakeStringSection((const char*)src, (const char*)PtrAdd(src, srcType.GetSize())),
                        buffer2, sizeof(buffer2));
                    if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                    if (!ImpliedTyping::Cast(
                        MakeIteratorRange(buffer), destType, 
                        MakeIteratorRange(buffer2), parsedType))
                        return false;
                } else {
                    if (!ImpliedTyping::Cast(
                        MakeIteratorRange(buffer), destType, 
                        { src, PtrAdd(src, srcType.GetSize()) }, srcType))
                        return false;
                }
            
                setFn(*(typename std::remove_reference<Type>::type*)obj, *(const PassToSetter*)buffer);
                return true;
            }

        template<typename SetFn, typename std::enable_if<SetterFnTraits<SetFn>::IsString>::type* = nullptr>
            bool DefaultCastFrom(
                void* obj, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
                std::function<SetFn> setFn)
            {
                static_assert(SetterFnTraits<SetFn>::IsArrayForm == false, "Mismatch on setter array form");
                using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
                using Type = typename SetterFnTraits<SetFn>::ObjectType;

                using DestCharType = typename PassToSetter::value_type;
				std::basic_string<DestCharType> str;
                if (srcStringForm) {
                    str = std::basic_string<DestCharType>(
                            (const DestCharType*)src,
                            (const DestCharType*)PtrAdd(src,srcType.GetSize()));
                } else
                    str = ImpliedTyping::AsString(src, srcType.GetSize(), srcType, true);
				setFn(*(typename std::remove_reference<Type>::type*)obj, str);
                return true;
            }

        template<typename SetFn, typename std::enable_if<!SetterFnTraits<SetFn>::IsString>::type* = nullptr>
            bool DefaultArrayCastFrom(
                void* obj, size_t arrayIndex, const void* src, ImpliedTyping::TypeDesc srcType, bool srcStringForm,
                std::function<SetFn> setFn)
            {
                static_assert(SetterFnTraits<SetFn>::IsArrayForm == true, "Mismatch on setter array form");
                using PassToSetter = typename std::remove_const<typename std::remove_reference<typename SetterFnTraits<SetFn>::ValueType>::type>::type;
                using Type = typename SetterFnTraits<SetFn>::ObjectType;

                char buffer[ParseBufferSize];
                auto destType = ImpliedTyping::TypeOf<PassToSetter>();
                if (srcStringForm) {
                    char buffer2[ParseBufferSize];
                    auto parsedType = ImpliedTyping::Parse(
                        MakeStringSection((const char*)src, (const char*)PtrAdd(src, srcType.GetSize())),
                        buffer2, sizeof(buffer2));
                    if (parsedType._type == ImpliedTyping::TypeCat::Void) return false;

                    if (!ImpliedTyping::Cast(
                        MakeIteratorRange(buffer), destType, 
                        MakeIteratorRange(buffer2), parsedType))
                        return false;
                } else {
                    if (!ImpliedTyping::Cast(
                        MakeIteratorRange(buffer), destType, 
                        { src, PtrAdd(src, srcType.GetSize()) }, srcType))
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
                using Type = typename SetterFnTraits<SetFn>::ObjectType;

                using DestCharType = typename PassToSetter::value_type;
				std::basic_string<DestCharType> str;
                if (srcStringForm) {
					str = std::basic_string<DestCharType>(
                            (const DestCharType*)src,
                            (const DestCharType*)PtrAdd(src,srcType.GetSize()));
                } else
                    str = ImpliedTyping::AsString(src, srcType.GetSize(), srcType, true);
				setFn(*(typename std::remove_reference<Type>::type*)obj, arrayIndex, str);
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
                return ImpliedTyping::Cast(
                    { dst, PtrAdd(dst, dstSize) }, dstType, 
                    AsOpaqueIteratorRange(src), ImpliedTyping::TypeOf<SrcType>());
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
                    MakeStringSection(src),
                    parseBuffer, sizeof(parseBuffer));
                if (parseType._type == ImpliedTyping::TypeCat::Void) return false;
                return ImpliedTyping::Cast(
                    { dst, PtrAdd(dst, dstSize) }, dstType, 
                    MakeIteratorRange(parseBuffer), parseType);
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
            //   A D D I N G   C A S T I N G   F U N C T I O N S

        class ClassAccessorsHelper
        {
        public:
            template<typename SetSig, typename std::enable_if<!SetterFnTraits<SetSig>::IsArrayForm>::type* = nullptr>
                static void MaybeAddCasterForSet(ClassAccessors& accessors, uint64 id, std::function<SetSig>&& setter);

            template<typename SetSig, typename std::enable_if<SetterFnTraits<SetSig>::IsArrayForm>::type* = nullptr>
                static void MaybeAddCasterForSet(ClassAccessors& accessors, uint64 id, std::function<SetSig>&& setter);

            template<typename GetSig, typename std::enable_if<!GetterFnTraits<GetSig>::IsArrayForm>::type* = nullptr>
                static void MaybeAddCasterForGet(ClassAccessors& accessors, uint64 id, std::function<GetSig>&& getter);

            template<typename GetSig, typename std::enable_if<GetterFnTraits<GetSig>::IsArrayForm>::type* = nullptr>
                static void MaybeAddCasterForGet(ClassAccessors& accessors, uint64 id, std::function<GetSig>&& getter);

            static void MaybeAddCasterForSet(ClassAccessors& accessors, uint64 id, std::function<void()>&& getter) {}
            static void MaybeAddCasterForGet(ClassAccessors& accessors, uint64 id, std::function<void()>&& getter) {}
        };
    
        template<typename SetSig, typename std::enable_if<!SetterFnTraits<SetSig>::IsArrayForm>::type*>
            void ClassAccessorsHelper::MaybeAddCasterForSet(
                ClassAccessors& accessors,
                uint64 id, std::function<SetSig>&& setter)
            {
                using namespace std::placeholders;
                accessors.PropertyForId(id)._castFrom = std::bind(&DefaultCastFrom<SetSig>, _1, _2, _3, _4, std::move(setter));
            }

        template<typename SetSig, typename std::enable_if<SetterFnTraits<SetSig>::IsArrayForm>::type*>
            void ClassAccessorsHelper::MaybeAddCasterForSet(
                ClassAccessors& accessors,
                uint64 id, std::function<SetSig>&& setter)
            {
                using namespace std::placeholders;
                accessors.PropertyForId(id)._castFromArray = std::bind(&DefaultArrayCastFrom<SetSig>, _1, _2, _3, _4, _5, std::move(setter));
            }

        template<typename GetSig, typename std::enable_if<!GetterFnTraits<GetSig>::IsArrayForm>::type*>
            void ClassAccessorsHelper::MaybeAddCasterForGet(
                ClassAccessors& accessors,
                uint64 id, std::function<GetSig>&& getter)
            {
                using namespace std::placeholders;
                accessors.PropertyForId(id)._castTo = std::bind(&DefaultCastTo<GetSig>, _1, _2, _3, _4, _5, std::move(getter));
            }

        template<typename GetSig, typename std::enable_if<GetterFnTraits<GetSig>::IsArrayForm>::type*>
            void ClassAccessorsHelper::MaybeAddCasterForGet(
                ClassAccessors& accessors,
                uint64 id, std::function<GetSig>&& getter)
            {
                using namespace std::placeholders;
                accessors.PropertyForId(id)._castToArray = std::bind(&DefaultArrayCastTo<GetSig>, _1, _2, _3, _4, _5, _6, std::move(getter));
            }

///////////////////////////////////////////////////////////////////////////////////////////////////

    }

    template<typename GetFn, typename SetFn>
        void ClassAccessors::Add(
            const char name[],
            GetFn&& getter, SetFn&& setter,
            const ImpliedTyping::TypeDesc& naturalType,
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
            p._naturalType = naturalType;
            p._fixedArrayLength = fixedArrayLength;

                // Generate casting functions
                // Casting functions always have the same signature
                //      --  this makes it easier to iterate through them
                //          during serialization (etc)
            
            Internal::ClassAccessorsHelper::MaybeAddCasterForSet(*this, id, std::move(scopy));
            Internal::ClassAccessorsHelper::MaybeAddCasterForGet(*this, id, std::move(gcopy));
        }

    template<typename ChildType, typename CreateFn, typename GetCountFn, typename GetByIndexFn, typename GetByKeyFn>
        void ClassAccessors::AddChildList(
            const char name[],
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
}

