module;

export module Gromada.DataExporters;

import std;
import utils;
import nlohmann_json_adapter; //TODO: try Glaze instead
import Gromada.Resources;

export { 
	void ExportMapToJson(const Map& map, std::ostream& stream);
	void ExportVidsToCsv(std::span<const VidRawData> vids, std::ostream& stream);
}


void ExportMapToJson(const Map& map, std::ostream& stream) {
	auto objectToJson = [](const DynamicObject& obj) {
		overloaded payloadVisitor{
			[](const DynamicObject::BasePayload& payload) { return nlohmann::json{{"hp", payload.hp}}; },
			[](const DynamicObject::AdvancedPayload& payload) {
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

	const auto& header = map.header();
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
		{"objects", map.objects() | std::views::transform(objectToJson) | std::views::common | std::ranges::to<std::vector>()},
	};

	stream << document;
}

template <typename T, auto MemberPtr> std::function<void(std::ostream&, const T&)> MakePrintFunction() {
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

	return [](std::ostream& stream, const T& obj) { Formatter(stream, std::invoke(MemberPtr, obj)); };
}


void ExportVidsToCsv(std::span<const VidRawData> vids, std::ostream& stream) {
	static std::initializer_list<std::pair<std::string_view, std::function<void(std::ostream&, const VidRawData&)>>> printFunctions = {
		{"name", MakePrintFunction<VidRawData, &VidRawData::name>()},
		{"unitType", MakePrintFunction<VidRawData, &VidRawData::unitType>()},
		{"behave", MakePrintFunction<VidRawData, &VidRawData::behave>()},
		{"flags", MakePrintFunction<VidRawData, &VidRawData::flags>()},
		{"collisionMask", MakePrintFunction<VidRawData, &VidRawData::collisionMask>()},
		{"anotherWidth", MakePrintFunction<VidRawData, &VidRawData::anotherWidth>()},
		{"anotherHeight", MakePrintFunction<VidRawData, &VidRawData::anotherHeight>()},
		{"z_or_height", MakePrintFunction<VidRawData, &VidRawData::z_or_height>()},
		{"maxHP", MakePrintFunction<VidRawData, &VidRawData::maxHP>()},
		{"gridRadius", MakePrintFunction<VidRawData, &VidRawData::gridRadius>()},
		{"p6", MakePrintFunction<VidRawData, &VidRawData::p6>()},
		{"speed", MakePrintFunction<VidRawData, &VidRawData::speed>()},
		{"hz1", MakePrintFunction<VidRawData, &VidRawData::hz1>()},
		{"hz2", MakePrintFunction<VidRawData, &VidRawData::hz2>()},
		{"hz3", MakePrintFunction<VidRawData, &VidRawData::hz3>()},
		{"army", MakePrintFunction<VidRawData, &VidRawData::army>()},
		{"someWeaponIndex", MakePrintFunction<VidRawData, &VidRawData::someWeaponIndex>()},
		{"hz4", MakePrintFunction<VidRawData, &VidRawData::hz4>()},
		{"deathSizeMargin", MakePrintFunction<VidRawData, &VidRawData::deathSizeMargin>()},
		{"somethingAboutDeath", MakePrintFunction<VidRawData, &VidRawData::somethingAboutDeath>()},
		{"sX", MakePrintFunction<VidRawData, &VidRawData::sX>()},
		{"sY", MakePrintFunction<VidRawData, &VidRawData::sY>()},
		{"sZ", MakePrintFunction<VidRawData, &VidRawData::sZ>()},
		{"hz5", MakePrintFunction<VidRawData, &VidRawData::hz5>()},
		{"hz6", MakePrintFunction<VidRawData, &VidRawData::hz6>()},
		{"Directions count", MakePrintFunction<VidRawData, &VidRawData::directionsCount>()},
		{"z", MakePrintFunction<VidRawData, &VidRawData::z>()},
		{"dataSizeOrNvid", MakePrintFunction<VidRawData, &VidRawData::dataSizeOrNvid>()},
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