// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/MemoryUtils.h"
#include "../Utility/UTFUtils.h"
#include <string>
#include <sstream>
#include <any>

template<typename Type>
	inline auto MakeStoreableInAny(const Type& type) { return type; }

template<typename CharType>
	inline auto MakeStoreableInAny(StringSection<CharType> type) { return type.AsString(); }

inline auto MakeStoreableInAny(const char* type) { return std::string(type); }
inline auto MakeStoreableInAny(const utf16* type) { return std::u16string(type); }
inline auto MakeStoreableInAny(const utf32* type) { return std::u32string(type); }

template<typename IteratorType>
	inline auto MakeStoreableInAny(IteratorRange<IteratorType> type) { static_assert("IteratorRange<>s cannot be stored in an std::any. Try explicitly creating a std::vector instead"); }

template<typename Type>
	std::ostream& MakeArchivableName(std::ostream& str, const Type& t) { return str << t; }
	
namespace Assets 
{
	namespace Internal
	{
		template<typename Param, typename std::enable_if<std::is_integral<Param>::value>::type* = nullptr>
			uint64_t HashParam_Chain(const Param& p, uint64_t seed) { return HashCombine(p, seed); }

		template<typename Param, decltype(std::declval<Param>().GetHash())* = nullptr>
			uint64_t HashParam_Chain(const Param& p, uint64_t seed) { return HashCombine(p.GetHash(), seed); }

		template<typename Param, decltype(std::declval<Param>()->GetHash())* = nullptr>
			uint64_t HashParam_Chain(const Param& p, uint64_t seed) { return HashCombine(p->GetHash(), seed); }

		template<typename Param, decltype(Hash64(std::declval<const Param&>(), 0ull))* = nullptr>
			uint64_t HashParam_Chain(const Param& p, uint64_t seed) { return Hash64(p, seed); }

		template<typename Param, typename std::enable_if<std::is_integral<Param>::value>::type* = nullptr>
			uint64_t HashParam_Single(const Param& p) { return IntegerHash64(p); }

		template<typename Param, decltype(std::declval<Param>().GetHash())* = nullptr>
			uint64_t HashParam_Single(const Param& p) { return p.GetHash(); }

		template<typename Param, decltype(std::declval<Param>()->GetHash())* = nullptr>
			uint64_t HashParam_Single(const Param& p) { return p->GetHash(); }

		template<typename Param, decltype(Hash64(std::declval<const Param&>(), 0ull))* = nullptr>
			uint64_t HashParam_Single(const Param& p) { return Hash64(p); }

		template <typename FirstParam, typename... Params>
			uint64_t BuildParamHash(FirstParam firstInitializer, Params... initialisers)
		{
				//  Note Hash64 is a relatively expensive hash function
				//      ... we might get away with using a simpler/quicker hash function
			uint64_t result = HashParam_Single(firstInitializer);
			int dummy[] = { 0, (result = HashParam_Chain(initialisers, result), 0)... };
			(void)dummy;
			return result;
		}

		inline uint64_t BuildParamHash() { return 0; }

		T1(Type) static auto IsStreamable(int) -> decltype(std::declval<std::ostream&>() << std::declval<const Type&>(), std::true_type{});
		T1(Type)  static auto IsStreamable(...) -> std::false_type;

		template<typename Type>
			static decltype(std::declval<std::ostream&>() << std::declval<const Type&>())
				StreamWithHashFallback(std::ostream& str, const Type& value) { return str << value; }

		template<typename Type>
			std::enable_if_t<!decltype(IsStreamable<Type>(0))::value, std::ostream>& StreamWithHashFallback(std::ostream& str, const Type& value)
			{
				return str << HashParam_Single(value);
			}

		template <typename Object>
			inline void StreamDashSeparated(std::basic_stringstream<char>& result, const Object& obj)
		{
			result << "-";
			StreamWithHashFallback(result, obj);
		}

		template <typename P0, typename... Params>
			std::basic_string<char> AsString(P0 p0, Params... initialisers)
		{
			std::basic_stringstream<char> result;
			result << p0;
			int dummy[] = { 0, (StreamDashSeparated(result, initialisers), 0)... };
			(void)dummy;
			return result.str();
		}

		inline std::basic_string<char> AsString() { return {}; }

		template<std::size_t idx=0, typename... Args>
			std::ostream& MakeArchivableName_Pack(std::ostream& str, const std::vector<std::any>& variantPack)
		{
			if constexpr (sizeof...(Args) == 0) return str;

			if (idx != 0) str << "-";
			using TT = std::tuple<Args...>;
			const auto& value = std::any_cast<const std::tuple_element_t<idx, TT>&>(variantPack[idx]);
			StreamWithHashFallback(str, value);
			if constexpr ((idx+1) != sizeof...(Args)) {
				return MakeArchivableName_Pack<idx+1, Args...>(str, variantPack);
			} else
				return str;
		}

		template<typename FirstArg, typename... Args>
			uint64_t MakeArchivableHash_Pack(const std::vector<std::any>& variantPack, uint64_t seed)
		{
			auto iterator = variantPack.begin();
			uint64_t result = HashParam_Single(std::any_cast<const FirstArg&>(*iterator++));			
			int dummy[] = { 0, (result = HashParam_Chain(std::any_cast<const Args&>(*iterator++), result), 0)... };
			(void)dummy; (void)iterator;
			assert(iterator == variantPack.end());
            return result;
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	class InitializerPack
	{
	public:
		std::string ArchivableName() const
		{ 
			std::stringstream str;
			_nameFn(str, _variantPack);
			return str.str();
		}

		uint64_t ArchivableHash() const
		{ 
			return _hashFn(_variantPack, DefaultSeed64);
		}

		template<typename Type>
			const Type& GetInitializer(unsigned idx) const
			{
				return std::any_cast<const Type&>(_variantPack[idx]);
			}

		std::size_t GetCount() const { return _variantPack.size(); }
		bool IsEmpty() const { return _variantPack.empty(); }

		template<typename... Args>
			InitializerPack(Args... args)
		: _variantPack { MakeStoreableInAny(args)... }
		{
			_nameFn = &Internal::MakeArchivableName_Pack<0, decltype(MakeStoreableInAny(std::declval<Args>()))...>;
			_hashFn = &Internal::MakeArchivableHash_Pack<decltype(MakeStoreableInAny(std::declval<Args>()))...>;
		}

	private:
		std::vector<std::any> _variantPack;

		using MakeArchivableNameFn = std::ostream& (*)(std::ostream&, const std::vector<std::any>&);
		using MakeArchivableHashFn = uint64_t (*)(const std::vector<std::any>&, uint64_t);
		MakeArchivableNameFn _nameFn;
		MakeArchivableHashFn _hashFn;
	};
}

