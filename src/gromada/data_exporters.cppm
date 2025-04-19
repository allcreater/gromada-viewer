module;

export module Gromada.DataExporters;

import std;
import utils;
import nlohmann_json_adapter; //TODO: try Glaze instead
import Gromada.Resources;

export { 
	void ExportMapToJson(const Map& map, std::ostream& stream);
	void ExportVidsToCsv(std::span<const Vid> vids, std::ostream& stream);
}


void ExportMapToJson(const Map& map, std::ostream& stream) {
	auto objectToJson = [](const GameObject& obj) {
		overloaded payloadVisitor{
			[](const GameObject::BasePayload& payload) { return nlohmann::json{{"hp", payload.hp}}; },
			[](const GameObject::AdvancedPayload& payload) {
				return nlohmann::json{
					{"hp", payload.hp},
					//{"buildTime", payload.buildTime},
					//{"army", payload.army},
					payload.buildTime.transform([](std::uint8_t x) { return nlohmann::json{"buildTime", x}; }).value_or(nlohmann::json{}),
					payload.army.transform([](std::uint8_t x) { return nlohmann::json{"army", x}; }).value_or(nlohmann::json{}),
					{"buildTime", payload.buildTime.value_or(0)},
					{"behave", payload.behave},
					{"items", payload.items},
				};
			},
			[](const std::monostate&) { return nlohmann::json{}; },
		};

		return nlohmann::json{
			{"nvid", obj.nvid},
			{"x", obj.x},
			{"y", obj.y},
			{"z", obj.z},
			{"direction", obj.direction},
			{"payload", std::visit(payloadVisitor, obj.payload)},
		};
	};

	const auto& header = map.header;
	nlohmann::json document{
		{"header",
			nlohmann::json{
				{"width", header.width},
				{"height", header.height},
				{"observerX", header.observerX},
				{"observerY", header.observerY},
				{"e", header.e},
				{"f", header.f},
				{"startTimer", header.startTimer},
				{"mapVersion", header.mapVersion},
			}},
		{"objects", map.objects | std::views::transform(objectToJson) | std::views::common | std::ranges::to<std::vector>()},
	};

	stream << document;
}

template <auto MemberPtr> 
auto MakePrintFunction() {
	constexpr static auto Formatter = [](std::ostream& stream, const auto& value) {
		using ValueType = std::decay_t<decltype(value)>;
		if constexpr (std::ranges::contiguous_range<ValueType>) {
			stream << std::string_view{value.data(), std::char_traits<char>::find(value.data(), value.size(), '\0')};
		}
		else if constexpr (std::is_integral_v<ValueType>) {
			stream << +value;
		}
		else {
			stream << "???";
		}
	};

	return std::function {
		[](std::ostream& stream, std::add_const_t<type_from_member_t<decltype(MemberPtr)>> obj) { Formatter(stream, std::invoke(MemberPtr, obj)); }
	};
}


void ExportVidsToCsv(std::span<const Vid> vids, std::ostream& stream) {
	static std::initializer_list<std::pair<std::string_view, std::function<void(std::ostream&, const Vid&)>>> printFunctions = {
		{"name", MakePrintFunction<&Vid::name>()},
		{"unitType", MakePrintFunction<&Vid::unitType>()},
		{"behave", MakePrintFunction<&Vid::behave>()},
		{"flags", MakePrintFunction<&Vid::flags>()},
		{"collisionMask", MakePrintFunction<&Vid::collisionMask>()},
		{"anotherWidth", MakePrintFunction<&Vid::anotherWidth>()},
		{"anotherHeight", MakePrintFunction<&Vid::anotherHeight>()},
		{"z_or_height", MakePrintFunction<&Vid::z_or_height>()},
		{"maxHP", MakePrintFunction<&Vid::maxHP>()},
		{"gridRadius", MakePrintFunction<&Vid::gridRadius>()},
		{"p6", MakePrintFunction<&Vid::p6>()},
		{"speedX", MakePrintFunction<&Vid::speedX>()},
		{"speedY", MakePrintFunction<&Vid::speedY>()},
		{"acceleration", MakePrintFunction<&Vid::acceleration>()},
		{"rotationPeriod", MakePrintFunction<&Vid::rotationPeriod>()},
		{"army", MakePrintFunction<&Vid::army>()},
		{"someWeaponIndex", MakePrintFunction<&Vid::someWeaponIndex>()},
		{"hz4", MakePrintFunction<&Vid::hz4>()},
		{"deathDamageRadius", MakePrintFunction<&Vid::deathDamageRadius>()},
		{"deathDamage", MakePrintFunction<&Vid::deathDamage>()},
		{"linkX", MakePrintFunction<&Vid::linkX>()},
		{"linkY", MakePrintFunction<&Vid::linkY>()},
		{"linkZ", MakePrintFunction<&Vid::linkZ>()},
		{"linkedObjectVid", MakePrintFunction<&Vid::linkedObjectVid>()},
		{"hz6", MakePrintFunction<&Vid::hz6>()},
		{"Directions count", MakePrintFunction<&Vid::directionsCount>()},
		{"z", MakePrintFunction<&Vid::z>()},
		{"dataSizeOrNvid", MakePrintFunction<&Vid::dataSizeOrNvid>()},
		//{"visualBehavior", MakePrintFunction<VidGraphics, &VidGraphics::visualBehavior>()},
		//{"hz7", MakePrintFunction<VidGraphics, &VidGraphics::hz7>()},
		//{"numOfFrames", MakePrintFunction<VidGraphics, &VidGraphics::numOfFrames>()},
		//{"dataSize", MakePrintFunction<VidGraphics, &VidGraphics::dataSize>()},
		//{"imgWidth", MakePrintFunction<VidGraphics, &VidGraphics::imgWidth>()},
		//{"imgHeight", MakePrintFunction<VidGraphics, &VidGraphics::imgHeight>()},
	};

	for (const auto& [name, _] : printFunctions) {
		stream << name << ',';
	}

	for (int index = 0; const auto& vid : vids) {
		stream << index;
		for (const auto& [_, printFunction] : printFunctions) {
			stream << ',';
			printFunction(stream, vid);
		}
		stream << '\n';
		index++;
	}
}