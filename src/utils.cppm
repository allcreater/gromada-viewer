export module utils;

import std;

export {

	template <typename... Ts> struct overloaded : Ts... {
		using Ts::operator()...;
	};


	struct SpanStreamReader {
		SpanStreamReader(std::span<const std::byte> data)
			: data{data} {}
		template <typename T> T read() {
			T result;
			std::memcpy(&result, data.data(), sizeof result);
			data = data.subspan(sizeof result);
			return result;
		}
		operator std::span<const std::byte>() const { return data; }

	private:
		std::span<const std::byte> data;
	};


	template <typename T> struct type_from_member;
	template <typename M, typename T> struct type_from_member<M T::*> {
		using type = T;
	};
	template <typename T> using type_from_member_t = type_from_member<T>::type;
}