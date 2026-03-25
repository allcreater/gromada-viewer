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

			std::memcpy(out.data(), data.data(), out.size_bytes());
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
}
