export module utils;

export {

	template <typename... Ts> struct overloaded : Ts... {
		using Ts::operator()...;
	};
}