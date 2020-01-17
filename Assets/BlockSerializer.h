// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "../Core/Exceptions.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/Streams/Serialization.h"
#include <vector>
#include <iterator>
#include <type_traits>

namespace Serialization
{

        ////////////////////////////////////////////////////

    class NascentBlockSerializer
    {
    public:
        struct SpecialBuffer
        {
            enum Enum { Unknown, VertexBuffer, IndexBuffer, String, Vector, UniquePtr };
        };
        
        template<typename Type> void    SerializeSubBlock(const Type* type);
        template<typename Type, typename std::enable_if< !std::is_pod<Type>::value >::type* = nullptr>
            void    SerializeSubBlock(IteratorRange<Type*> range, SpecialBuffer::Enum specialBuffer = SpecialBuffer::Unknown);

        template<typename Type, typename std::enable_if< std::is_pod<Type>::value >::type* = nullptr>
            void    SerializeSubBlock(IteratorRange<Type*> range, SpecialBuffer::Enum specialBuffer = SpecialBuffer::Unknown);

        void    SerializeSubBlock(NascentBlockSerializer& subBlock, SpecialBuffer::Enum specialBuffer = SpecialBuffer::Unknown);
        void    SerializeRawSubBlock(IteratorRange<const void*> range, SpecialBuffer::Enum specialBuffer = SpecialBuffer::Unknown);

        void    SerializeSpecialBuffer(SpecialBuffer::Enum specialBuffer, IteratorRange<const void*> range);
        
        void    SerializeValue  ( uint8     value );
        void    SerializeValue  ( uint16    value );
        void    SerializeValue  ( uint32    value );
        void    SerializeValue  ( uint64    value );
        void    SerializeValue  ( float     value );
        void    SerializeValue  ( const std::string& value );
        void    AddPadding      ( unsigned sizeInBytes );

        template<typename Type, typename Allocator>
            void    SerializeRaw    ( const std::vector<Type, Allocator>& value );

		template<typename Type>
            void    SerializeRaw    ( const SerializableVector<Type>& value );

		template<typename Type, size_t Count>
			void    SerializeRaw	( Type      (&type)[Count] ); 
			
		template<typename Type>
            void    SerializeRaw    ( Type      type );

        std::unique_ptr<uint8[]>        AsMemoryBlock() const;
        size_t                          Size() const;

        NascentBlockSerializer();
        ~NascentBlockSerializer();

        class InternalPointer
        {
        public:
            uint64_t                _pointerOffset;
            uint64_t                _subBlockOffset;
            uint64_t                _subBlockSize;
            #ifndef APPORTABLE
                uint64_t                _specialBuffer;     // (this is SpecialBuffer::Enum. Made uint64_t to make alignment consistant across all platforms)
            #else
                SpecialBuffer::Enum     _specialBuffer;
            #endif
        };

        static const size_t PtrFlagBit  = size_t(1)<<(size_t(sizeof(size_t)*8-1));
        static const size_t PtrMask     = ~PtrFlagBit;

    protected:
        std::vector<uint8>              _memory;
        std::vector<uint8>              _trailingSubBlocks;
        std::vector<InternalPointer>    _internalPointers;

        void PushBackPointer(size_t value);
        void PushBackRaw(const void* data, size_t size);
        void PushBackRaw_SubBlock(const void* data, size_t size);
        void RegisterInternalPointer(const InternalPointer& ptr);
        void PushBackPlaceholder(SpecialBuffer::Enum specialBuffer);
    };

    void            Block_Initialize(void* block, const void* base=nullptr);
    const void*     Block_GetFirstObject(const void* blockStart);
    size_t          Block_GetSize(const void* block);
    std::unique_ptr<uint8[]>     Block_Duplicate(const void* block);

        ////////////////////////////////////////////////////

    namespace Internal
    {
        template<typename T> struct HasSerializeMethod
        {
            template<typename U, void (U::*)(NascentBlockSerializer&) const> struct FunctionSignature {};
            template<typename U> static std::true_type Test1(FunctionSignature<U, &U::SerializeMethod>*);
            template<typename U> static std::false_type Test1(...);
            static const bool Result = decltype(Test1<T>(0))::value;
        };

        template<typename T> struct IsValueType
        {
            template<typename U, void (NascentBlockSerializer::*)(U)> struct FunctionSignature {};
            template<typename U> static std::true_type Test1(FunctionSignature<U, &NascentBlockSerializer::SerializeValue>*);
            template<typename U> static std::false_type Test1(...);
            static const bool Result = decltype(Test1<T>(0))::value | decltype(Test1<const T&>(0))::value;
        };
    }

	template<typename Type, typename std::enable_if<Serialization::Internal::HasSerializeMethod<Type>::Result>::type* = nullptr>
		void Serialize(NascentBlockSerializer& serializer, const Type& value)
			{ value.SerializeMethod(serializer); }

	template <typename Type, typename std::enable_if<Serialization::Internal::IsValueType<Type>::Result>::type* = nullptr>
		void Serialize(NascentBlockSerializer& serializer, const Type& value)
			{ serializer.SerializeValue(value); }

	template <typename Type, typename std::enable_if<std::is_same<const bool, decltype(Type::SerializeRaw)>::value && Type::SerializeRaw>::type* = nullptr>
		void Serialize(NascentBlockSerializer& serializer, const Type& value)
			{ serializer.SerializeRaw(value); }

	template<typename Type, typename Allocator>
        void    Serialize  ( NascentBlockSerializer& serializer, const std::vector<Type, Allocator>& value )
    {
        serializer.SerializeSubBlock(MakeIteratorRange(value), NascentBlockSerializer::SpecialBuffer::Vector);
    }

	template<typename Type>
        void    Serialize  ( NascentBlockSerializer& serializer,const SerializableVector<Type>& value )
    {
        serializer.SerializeSubBlock(MakeIteratorRange(value), NascentBlockSerializer::SpecialBuffer::Vector);
    }

    template<typename Type, typename Deletor>
        void    Serialize  ( NascentBlockSerializer& serializer,const DynamicArray<Type, Deletor>& value )
    {
        serializer.SerializeSubBlock(MakeIteratorRange(value), NascentBlockSerializer::SpecialBuffer::UniquePtr);
        SerializeValue(value.size());
    }
        
    template<typename Type, typename Deletor>
        void    Serialize  ( NascentBlockSerializer& serializer,const std::unique_ptr<Type, Deletor>& value, size_t count )
    {
        serializer.SerializeSubBlock(MakeIteratorRange(value.get(), &value[count]), NascentBlockSerializer::SpecialBuffer::UniquePtr);
    }

	template<int Dimen, typename Primitive>
		inline void Serialize(  NascentBlockSerializer& serializer,
								const cml::vector< Primitive, cml::fixed<Dimen> >& vec)
	{
		for (unsigned j=0; j<Dimen; ++j) {
			Serialize(serializer, vec[j]);
		}
	}
    
	inline void Serialize(  NascentBlockSerializer&  serializer,
							const ::XLEMath::Float4x4&              float4x4)
	{
		for (unsigned i=0; i<4; ++i)
			for (unsigned j=0; j<4; ++j) {
				Serialize(serializer, float4x4(i,j));
			}
	}

	template<typename TypeLHS, typename TypeRHS>
		void Serialize(NascentBlockSerializer& serializer, const std::pair<TypeLHS, TypeRHS>& value)
			{ 
				Serialize(serializer, value.first);
				Serialize(serializer, value.second);
				const auto padding = sizeof(typename std::pair<TypeLHS, TypeRHS>) - sizeof(TypeLHS) - sizeof(TypeRHS);
				if (constant_expression<(padding > 0)>::result()) {
					serializer.AddPadding(padding);
				}
			}

		// the following has no implementation. Objects that don't match will attempt to use this implementation
	void Serialize(NascentBlockSerializer& serializer, ...) = delete;

        ////////////////////////////////////////////////////

    template<typename Type, typename std::enable_if< !std::is_pod<Type>::value >::type*>
        void    NascentBlockSerializer::SerializeSubBlock(IteratorRange<Type*> range, SpecialBuffer::Enum specialBuffer)
    {
        NascentBlockSerializer temporaryBlock;
        for (const auto& i:range) Serialize(temporaryBlock, i);
        SerializeSubBlock(temporaryBlock, specialBuffer);
    }

    template<typename Type, typename std::enable_if< std::is_pod<Type>::value >::type*>
        void    NascentBlockSerializer::SerializeSubBlock(IteratorRange<Type*> range, SpecialBuffer::Enum specialBuffer)
    {
        SerializeRawSubBlock(IteratorRange<const void*>((const void*)range.begin(), (const void*)range.end()), specialBuffer);
    }
        
    template<typename Type>
        void    NascentBlockSerializer::SerializeSubBlock(const Type* type)
    {
        NascentBlockSerializer temporaryBlock;
        Serialize(temporaryBlock, type);
        SerializeSubBlock(temporaryBlock, SpecialBuffer::Unknown);
    }

    template<typename Type, typename Allocator>
        void    NascentBlockSerializer::SerializeRaw(const std::vector<Type, Allocator>& vector)
    {
            // serialize the vector using just a raw copy of the contents
        SerializeRawSubBlock(MakeIteratorRange(vector), Serialization::NascentBlockSerializer::SpecialBuffer::Vector);
    }

	template<typename Type>
        void    NascentBlockSerializer::SerializeRaw(const SerializableVector<Type>& vector)
    {
            // serialize the vector using just a raw copy of the contents
        SerializeRawSubBlock(MakeIteratorRange(vector), Serialization::NascentBlockSerializer::SpecialBuffer::Vector);
    }

	template<typename Type, size_t Count>
		void    NascentBlockSerializer::SerializeRaw(Type(&type)[Count])
	{
		PushBackRaw(type, sizeof(Type)*Count);
	}

    template<typename Type>
        void    NascentBlockSerializer::SerializeRaw(Type      type)
    {
        PushBackRaw(&type, sizeof(Type));
    }
}

