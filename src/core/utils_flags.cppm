module;
#include <cassert>

export module utils:flags;

import std;

export template <typename Enum>
requires std::is_enum_v<Enum>
class Flags {
public:
    using underlying_type = std::underlying_type_t<Enum>;

    constexpr Flags() noexcept = default;
    explicit constexpr Flags(std::convertible_to<underlying_type> auto raw_value) noexcept : m_value(static_cast<underlying_type>(raw_value)) {
    	assert(static_cast<decltype(raw_value)>(m_value) == raw_value && "Flags: raw value is not representable in underlying type");
    }

    template <typename... Enums>
    requires (std::same_as<Enum, Enums> && ...)
    constexpr Flags(Enums... flags) noexcept : m_value{ (static_cast<underlying_type>(flags) | ...) } {}

    constexpr operator Enum() const noexcept { return static_cast<Enum>(m_value); }

    constexpr bool operator[](Enum flag) const noexcept {
    	assert(std::popcount( static_cast<underlying_type>(flag)) == 1 && "Flags can only be checked for single flags, not combinations");
		return m_value & static_cast<underlying_type>(flag);
	}

    friend constexpr Flags operator &(Flags lhs, Flags rhs) noexcept {
		return Flags{lhs.m_value & rhs.m_value};
	}
    friend constexpr Flags operator |(Flags lhs, Flags rhs) noexcept {
    	return Flags{lhs.m_value | rhs.m_value};
    }
    friend constexpr Flags operator ^(Flags lhs, Flags rhs) noexcept {
    	return Flags{lhs.m_value ^ rhs.m_value};
    }

    constexpr void visitActiveFlags(std::invocable<Enum> auto&& visitor) const {
		for (int bit = 0; bit < std::numeric_limits<underlying_type>::digits; ++bit) {
			const auto mask = static_cast<underlying_type>(underlying_type{1} << bit);
			if (!(m_value & mask)) continue;

			visitor(static_cast<Enum>(mask));
		}
	}

	// Generic "None" / "A, B, C" rendering for any flags enum that has a to_string(Enum) for its individual bits.
	friend std::string to_string(Flags flags)
	requires requires (Enum flag) { { to_string(flag) } -> std::convertible_to<std::string>; }
	{
		if (flags.m_value == 0) {
			return "None";
		}

		std::string result;
		flags.visitActiveFlags([&result](Enum flag) {
			if (!result.empty()) {
				result += ", ";
			}
			result += to_string(flag);
		});

		return result;
	}

private:
    underlying_type m_value = 0;
};

template <typename HeadEnum, typename... Enums>
Flags(HeadEnum, Enums...) -> Flags<HeadEnum>;
