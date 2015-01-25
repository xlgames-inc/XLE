// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"
#include <memory>
#include <vector>
#include <assert.h>

#if COMPILER_ACTIVE == COMPILER_TYPE_MSVC

	#if _MSC_VER >= 1700
		#define VARIADIC_TEMPLATE_PARAMETERS
		#define STD_LIB_HAS_MAKE_UNIQUE
		#define STD_LIB_HAS_FORWARD
	#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////

        //
        //   std::make_unique implementation from C++14 proposals at:
        //      http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3588.txt
        //

	#if !defined(STD_LIB_HAS_MAKE_UNIQUE)

		#if defined(VARIADIC_TEMPLATE_PARAMETERS)

			namespace std {
				template<class T> struct _Never_true : false_type { };

				template<class T> struct _Unique_if {
					typedef unique_ptr<T> _Single;
				};

				template<class T> struct _Unique_if<T[]> {
					typedef unique_ptr<T[]> _Runtime;
				};

				template<class T, size_t N> struct _Unique_if<T[N]> {
					static_assert(_Never_true<T>::value, "make_unique forbids T[N]. Please use T[].");
				};

				template<class T, class... Args> typename _Unique_if<T>::_Single make_unique(Args&&... args) {
					return unique_ptr<T>(new T(std::forward<Args>(args)...));
				}

				template<class T> typename _Unique_if<T>::_Single make_unique_default_init() {
					return unique_ptr<T>(new T);
				}

				template<class T> typename _Unique_if<T>::_Runtime make_unique(size_t n) {
					typedef typename remove_extent<T>::type U;
					return unique_ptr<T>(new U[n]());
				}

				template<class T> typename _Unique_if<T>::_Runtime make_unique_default_init(size_t n) {
					typedef typename remove_extent<T>::type U;
					return unique_ptr<T>(new U[n]);
				}

				template<class T, class... Args> typename _Unique_if<T>::_Runtime make_unique_value_init(size_t n, Args&&... args) {
					typedef typename remove_extent<T>::type U;
					return unique_ptr<T>(new U[n]{ std::forward<Args>(args)... });
				}

				template<class T, class... Args> typename _Unique_if<T>::_Runtime make_unique_auto_size(Args&&... args) {
					typedef typename remove_extent<T>::type U;
					return unique_ptr<T>(new U[sizeof...(Args)]{ std::forward<Args>(args)... });
				}
			}

		#else

			namespace std {

					//
					//      Primitive implementation of make_unique with limited parameter support.
					//      This is required to get around lack of support for variadic template
					//      parameters most version of the VS compiler.
					//

				template <typename Result>
					std::unique_ptr<Result> make_unique() { return std::unique_ptr<Result>(new Result()); }

				template <typename Result, typename X1>
					std::unique_ptr<Result> make_unique(X1 x1) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1))); }

				template <typename Result, typename X1, typename X2>
					std::unique_ptr<Result> make_unique(X1 x1, X2 x2) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1), std::forward<X2>(x2))); }

				template <typename Result, typename X1, typename X2, typename X3>
					std::unique_ptr<Result> make_unique(X1 x1, X2 x2, X3 x3) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3))); }

				template <typename Result, typename X1, typename X2, typename X3, typename X4>
					std::unique_ptr<Result> make_unique(X1 x1, X2 x2, X3 x3, X4 x4) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4))); }

				template <typename Result, typename X1, typename X2, typename X3, typename X4, typename X5>
					std::unique_ptr<Result> make_unique(X1 x1, X2 x2, X3 x3, X4 x4, X5 x5) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4), std::forward<X5>(x5))); }

				template <typename Result, typename X1, typename X2, typename X3, typename X4, typename X5, typename X6>
					std::unique_ptr<Result> make_unique(X1 x1, X2 x2, X3 x3, X4 x4, X5 x5, X6 x6) { return std::unique_ptr<Result>(new Result(std::forward<X1>(x1), std::forward<X2>(x2), std::forward<X3>(x3), std::forward<X4>(x4), std::forward<X5>(x5), std::forward<X6>(x6))); }


				//
				//      (see also an interesting version from the comments in 
				//          http://herbsutter.com/gotw/_102/#comment-6428
				//      this uses macros from the VS2012 headers. Something similar
				//      might be possible using Loki libraries or boost libraries.)
				//
				// #define _MAKE_UNIQUE(TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4)  \
				//                                                                                 \
				//     template<class T COMMA LIST(_CLASS_TYPE)>>                                  \
				//         std::unique_ptr<T> make_unique(LIST(_TYPE_REFREF_ARG))                  \
				//         {                                                                       \
				//             return std::unique_ptr<T>(new T(LIST(_FORWARD_ARG)));               \
				//         }                                                                       \
				//                                                                                 \
				//     /**/
				// 
				// _VARIADIC_EXPAND_0X(_MAKE_UNIQUE, , , , )
				// #undef _MAKE_UNIQUE



				template<class T> struct _Never_true : false_type { };

				template<class T> struct _Unique_if {
					typedef unique_ptr<T> _Single;
				};

				template<class T> struct _Unique_if<T[]> {
					typedef unique_ptr<T[]> _Runtime;
				};

				template<class T, size_t N> struct _Unique_if<T[N]> {
					static_assert(_Never_true<T>::value, "make_unique forbids T[N]. Please use T[].");
				};

				template<class T> typename _Unique_if<T>::_Runtime make_unique(size_t n) 
				{
					typedef typename remove_extent<T>::type U;
					return unique_ptr<T>(new U[n]());
				}

				#if (TARGET_64BIT==1)
					template<class T> typename _Unique_if<T>::_Runtime make_unique(unsigned n)
					{
						typedef typename remove_extent<T>::type U;
						return unique_ptr<T>(new U[n]());
					}
				#endif
			}

		#endif
	#endif

	#if !defined(STD_LIB_HAS_FORWARD)

		#if _MSC_VER <= 1600
			namespace std
			{
					//
					//      From http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2951.html
					//      (recommended implementation of std::forward)
					//
				template<class T, class U/*,        this condition isn't compiling in VS2010
					class = 
						typename enable_if<
							(is_lvalue_reference<T>::value ? is_lvalue_reference<U>::value : true) &&
							is_convertible<typename remove_reference<U>::type*,typename remove_reference<T>::type*>::value
						>::type*/>
				inline T&& forward(U&& u)
				{
					return static_cast<T&&>(u);
				}
			}
		#endif

	#endif

#endif

namespace Utility
{
            /////////////////////////////////////////////////////////////////////////////

    template <typename Type>
        Type * PtrAdd( Type * input, ptrdiff_t offset )  { return (Type*)( size_t(input) + offset ); }

    template <typename Type> 
        Type * AsPointer( Type * i )                     { return i; }

    #if STL_ACTIVE == STL_MSVC

        template <typename VecType>
			typename VecType::value_type * AsPointer(const std::_Vector_iterator<VecType> & i)
                { return i._Ptr; }

        template <typename VecType>
			const typename VecType::value_type * AsPointer(const std::_Vector_const_iterator<VecType> & i)
                { return i._Ptr; }

		#if _MSC_VER >= 1700
			template <typename StringType>
				typename StringType::value_type * AsPointer(const std::_String_iterator<StringType>& i)
				{
					return (typename StringType::value_type*)i._Ptr;
				}

			template <typename StringType>
				const typename StringType::value_type * AsPointer(const std::_String_const_iterator<StringType>& i)
				{
					return (typename StringType::value_type*)i._Ptr;
				}
		#else
			template <typename Type, typename Traits, typename Alloc>
				Type * AsPointer(const std::_String_iterator<Type, Traits, Alloc>& i)
				{
					return (Type*)i._Ptr;
				}

			template <typename Type, typename Traits, typename Alloc>
				const Type * AsPointer(const std::_String_const_iterator<Type, Traits, Alloc>& i)
				{
					return (Type*)i._Ptr;
				}
		#endif

    #else

        template <typename Type> 
            Type * AsPointer( const typename std::vector<Type>::iterator & i )                   { return &(*i); }

        template <typename Type> 
            const Type * AsPointer( const typename std::vector<Type>::const_iterator & i )       { return &(*i); }

        template <typename Type> 
            Type * AsPointer( const __gnu_cxx::__normal_iterator<Type, std::vector<Type> > & i )       { return &(*i); }

    #endif


    template<typename DestinationType, typename SourceType>
        DestinationType checked_cast(SourceType source)
        {
                //
                //  Note -- if RTTI is disabled in Debug, then it's never checked
                //
            #if defined(_DEBUG) && FEATURE_RTTI
                DestinationType result = dynamic_cast<DestinationType>(source);
                assert(!source || result);
                return result;
            #else
                return static_cast<DestinationType>(source);
            #endif
        }

            ////////////////////////////////////////////////////////

    template<typename Type, typename Deletor = std::default_delete<Type[]>> class DynamicArray
    {
    public:
        typedef Type    value_type;

        Type*           begin() never_throws                            { return _elements.get(); }
        Type*           end() never_throws                              { return &_elements[_count]; }
        size_t          size() const never_throws                       { return _count; }
        Type&           operator[](unsigned index) never_throws         { assert(index < _count); return _elements[index]; }

        const Type*     begin() const never_throws                      { return _elements.get(); }
        const Type*     end() const never_throws                        { return &_elements[_count]; }
        const Type&     operator[](unsigned index) const never_throws   { assert(index < _count); return _elements[index]; }

        Type*           get() never_throws                              { return _elements.get(); }
        const Type*     get() const never_throws                        { return _elements.get(); }
        std::unique_ptr<Type[]> release() never_throws                  { _count = 0; return std::move(_elements); }

        DynamicArray(std::unique_ptr<Type[], Deletor>&& elements, size_t count);
        
        template<typename OtherDeletor>
            DynamicArray(DynamicArray<Type, OtherDeletor>&& moveFrom);
        template<typename OtherDeletor>
            DynamicArray& operator=(DynamicArray<Type, OtherDeletor>&& moveFrom);

        static DynamicArray<Type, Deletor> Copy(const DynamicArray<Type, Deletor>& copyFrom);

    private:
        std::unique_ptr<Type[], Deletor>    _elements;
        size_t                              _count;

        DynamicArray(const DynamicArray& copyFrom);
        DynamicArray& operator=(const DynamicArray& copyFrom);
    };

            ///////////////////////////

    template<typename Type, typename Deletor>
        DynamicArray<Type, Deletor>::DynamicArray(std::unique_ptr<Type[], Deletor>&& elements, size_t count)
        : _elements(std::forward<std::unique_ptr<Type[], Deletor>>(elements))
        , _count(count)
    {}

    template<typename Type, typename Deletor>
        template<typename OtherDeletor>
            DynamicArray<Type, Deletor>::DynamicArray(DynamicArray<Type, OtherDeletor>&& moveFrom)
            : _elements(std::move(moveFrom._elements))
            , _count(moveFrom._count)
    {}

    template<typename Type, typename Deletor>
        template<typename OtherDeletor>
            DynamicArray<Type, Deletor>& DynamicArray<Type, Deletor>::operator=(DynamicArray<Type, OtherDeletor>&& moveFrom)
    {
        _elements = std::move(moveFrom._elements);
        _count = moveFrom._count;
        return *this;
    }

    template<typename Type, typename Deletor>
        DynamicArray<Type, Deletor> DynamicArray<Type, Deletor>::Copy(const DynamicArray<Type, Deletor>& copyFrom)
    {
        if (!copyFrom._count || !copyFrom._elements) {
            return DynamicArray<Type, Deletor>(std::unique_ptr<Type[], Deletor>(), 0);
        }
        std::unique_ptr<Type[], Deletor> newElements(new Type[copyFrom._count]);
        std::copy(copyFrom.begin(), copyFrom.end(), newElements.get());
        return DynamicArray<Type, Deletor>(std::move(newElements), copyFrom._count);
    }
}

using namespace Utility;
