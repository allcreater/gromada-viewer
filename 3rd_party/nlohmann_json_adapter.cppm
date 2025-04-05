// wrapper proposed by @Ju1He1 https://github.com/nlohmann/json/issues/3970#issuecomment-2714697468
 
// Global module fragment where #includes can happen
module;

// included headers will not be exported to the client
#include <nlohmann/json.hpp>

// first thing after the Global module fragment must be a module command
export module nlohmann_json_adapter;

// build up namespace from macros
#define NLOHMANN_NS NLOHMANN_JSON_NAMESPACE_CONCAT(NLOHMANN_JSON_ABI_TAGS, NLOHMANN_JSON_NAMESPACE_VERSION)

export {
	namespace nlohmann::NLOHMANN_NS::detail {
		using ::nlohmann::NLOHMANN_NS::detail::json_sax_dom_callback_parser;
		using ::nlohmann::NLOHMANN_NS::detail::json_sax_dom_parser;
	}

	namespace nlohmann::NLOHMANN_NS::literals {
		using ::nlohmann::NLOHMANN_NS::literals::operator""_json;
		using ::nlohmann::NLOHMANN_NS::literals::operator""_json_pointer;
	}



	namespace nlohmann {
		using ::nlohmann::basic_json;
		using ::nlohmann::json;
		using ::nlohmann::json_sax;
		using ::nlohmann::ordered_json;
		using ::nlohmann::detail::json_sax_dom_parser;
		using ::nlohmann::detail::json_sax_dom_callback_parser;
		using ::nlohmann::to_json;
		using ::nlohmann::from_json;
	}
}

module :private;
// possible to include headers here, but they will not be exported to the client