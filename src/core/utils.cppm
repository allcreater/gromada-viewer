module;
#include <cassert>

export module utils;

import std;

export {

	template <typename... Ts> struct overloaded : Ts... {
		using Ts::operator()...;
	};

	template <typename T>
	concept StreamLike = requires(T t) {
		t.read_to(std::span<std::byte>{});
		{ t.bytesRemaining() } -> std::convertible_to<std::size_t>;
	};

	template <typename BaseType>
	struct StreamReaderMixin {
		template <typename T>
		requires std::is_trivially_copyable_v<T>
		void read_to(T& out) {
			self().read_to(std::span{ reinterpret_cast<std::byte*>(&out), sizeof (T) });
		}

		template <typename T>
		requires std::is_trivially_copyable_v<T>
		[[nodiscard]] T read() {
			T result;
			read_to(result);
			return result;
		}

		std::vector<std::byte> readAll() && {
			std::vector<std::byte> result(self().bytesRemaining());
			read_to(std::span{result});
			return result;
		}

	private:
		BaseType& self() {
			static_assert(StreamLike<BaseType>);
			return static_cast<BaseType&>(*this);
		}
	};

	struct SpanStreamReader : StreamReaderMixin<SpanStreamReader> {
		SpanStreamReader(std::span<const std::byte> data)
			: data{data} {}

		void read_to(std::span<std::byte> out) {
			if (data.size_bytes() < out.size_bytes()) [[unlikely]] {
				throw std::out_of_range("Not enough data to read");
			}

			std::copy(data.data(), data.data() + out.size_bytes(), out.data());
			data = data.subspan(out.size_bytes());
		}

	    std::span<const std::byte> read_bytes(std::size_t size) {
		    if (data.size_bytes() < size) [[unlikely]] {
		        throw std::out_of_range("Not enough data to read");
		    }

		    return std::exchange(data, data.subspan(size)).subspan(0, size);
		}

		void skip(std::size_t size) {
			auto _ = read_bytes(size);
		}

		operator std::span<const std::byte>() const { return data; }

		[[nodiscard]] std::size_t bytesRemaining() const noexcept { return data.size_bytes(); }

	private:
		std::span<const std::byte> data;
	};


	template <typename T> struct type_from_member;
	template <typename M, typename T> struct type_from_member<M T::*> {
		using type = T;
	};
	template <typename T> using type_from_member_t = type_from_member<T>::type;


    constexpr int ordering_to_int (std::strong_ordering ordering) {
        if (ordering < 0)
            return -1;
        else if (ordering > 0)
            return 1;

        return 0;
    }


	template <typename Enum>
	requires std::is_enum_v<Enum>
	class Flags {
    public:
    	using underlying_type = std::underlying_type_t<Enum>;

    	constexpr Flags() noexcept = default;
    	explicit constexpr Flags(underlying_type raw_value) noexcept : m_value(raw_value) {}

    	template <typename... Enums>
    	requires (std::same_as<Enum, Enums> && ...)
    	constexpr Flags(Enums... flags) noexcept : m_value{ (static_cast<underlying_type>(flags) | ...) } {}

    	constexpr operator Enum() const noexcept { return static_cast<Enum>(m_value); }

    	constexpr bool operator[](Enum flag) const noexcept {
    		assert(std::popcount( static_cast<underlying_type>(flag)) == 1 && "Flags can only be checked for single flags, not combinations");
			return m_value & static_cast<underlying_type>(flag);
		}

		// Generic "None" / "A, B, C" rendering for any flags enum that has a to_string(Enum) for its individual bits.
		friend std::string to_string(Flags flags)
		requires requires (Enum flag) { { to_string(flag) } -> std::convertible_to<std::string>; }
		{
			if (flags.m_value == 0) {
				return "None";
			}

			std::string result;
			for (int bit = 0; bit < std::numeric_limits<underlying_type>::digits; ++bit) {
				const auto mask = static_cast<underlying_type>(underlying_type{1} << bit);
				if (!(flags.m_value & mask)) continue;

				if (!result.empty()) {
					result += ", ";
				}
				result += to_string(static_cast<Enum>(mask));
			}

			return result;
		}

    private:
    	underlying_type m_value = 0;
    };

	template <typename HeadEnum, typename... Enums>
	Flags(HeadEnum, Enums...) -> Flags<HeadEnum>;

}
