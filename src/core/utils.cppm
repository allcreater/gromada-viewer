module;
#include <cassert>

export module utils;

import std;

export import :flags;
export import :streamreader;

export {

	template <typename... Ts> struct overloaded : Ts... {
		using Ts::operator()...;
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
