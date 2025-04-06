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
}