module;

export module Gromada.DataExporters;

import std;
import utils;
import nlohmann_json_adapter; //TODO: try Glaze instead
import Gromada.Resources;
import Gromada.Map;

export { 
	void ExportMapToJson(std::span<const Vid> vids, const Map& map, std::ostream& stream);
	void ExportVidsToCsv(std::span<const Vid> vids, std::ostream& stream);
}


void ExportMapToJson(std::span<const Vid> vids, const Map& map, std::ostream& stream) {
	auto objectToJson = [vids](const GameObject& obj) {
	    auto payloadToJson = [objectSerializationClass = getObjectSerializationClass(vids[obj.nvid].behave)](const GameObject::Payload& payload) {
	        switch (objectSerializationClass) {
	            case ObjectSerializationClass::Static:
                    return nlohmann::json{{"hp", payload.hp}};

	            case ObjectSerializationClass::Dynamic:
	                return nlohmann::json{
					        {"hp", payload.hp},
                            {"buildTime", payload.buildTime},
                            {"army", payload.army},
                            {"behave", payload.behave},
                            {"items", payload.items},
                        };

	            default:
	                return nlohmann::json{}; // No payload or unknown class
	        }
	    };

	    auto commandToJson = [](const ObjectCommand& command) {
	        return nlohmann::json{
	            {"opcode", command.command},
	            {"p1", command.p1},
	            {"p2", command.p2}
	        };
	    };

		return nlohmann::json::object({
			{"nvid", obj.nvid},
			{"x", obj.x},
			{"y", obj.y},
			{"z", obj.z},
			{"direction", obj.direction},
			{"payload", payloadToJson(obj.payload)},
		    {"id", obj.id},
		    {"commands", obj.payload.commands | std::views::transform(commandToJson) | std::ranges::to<std::vector>()},
		});
	};

	const auto& header = map.header;
	nlohmann::json document{
		{"header",
			nlohmann::json{
				{"width", header.width},
				{"height", header.height},
				{"observerX", header.observerX},
				{"observerY", header.observerY},
				{"scaleX", header.scaleX},
				{"scaleY", header.scaleY},
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
		{"sizeX", MakePrintFunction<&Vid::sizeX>()},
		{"sizeY", MakePrintFunction<&Vid::sizeY>()},
		{"sizeZ", MakePrintFunction<&Vid::sizeZ>()},
		{"maxHP", MakePrintFunction<&Vid::maxHP>()},
		{"gridRadius", MakePrintFunction<&Vid::gridRadius>()},
		{"unused1", MakePrintFunction<&Vid::unused1>()},
		{"speedX", MakePrintFunction<&Vid::speedX>()},
		{"speedY", MakePrintFunction<&Vid::speedY>()},
		{"acceleration", MakePrintFunction<&Vid::acceleration>()},
		{"rotationPeriod", MakePrintFunction<&Vid::rotationPeriod>()},
		{"army", MakePrintFunction<&Vid::army>()},
		{"someWeaponIndex", MakePrintFunction<&Vid::someWeaponIndex>()},
		{"unused2", MakePrintFunction<&Vid::unused2>()},
		{"deathDamageRadius", MakePrintFunction<&Vid::deathDamageRadius>()},
		{"deathDamage", MakePrintFunction<&Vid::deathDamage>()},
		{"linkX", MakePrintFunction<&Vid::linkX>()},
		{"linkY", MakePrintFunction<&Vid::linkY>()},
		{"linkZ", MakePrintFunction<&Vid::linkZ>()},
		{"linkedObjectVid", MakePrintFunction<&Vid::linkedObjectVid>()},
		{"unused3", MakePrintFunction<&Vid::unused3>()},
		{"Directions count", MakePrintFunction<&Vid::directionsCount>()},
		{"z_layer", MakePrintFunction<&Vid::z_layer>()},
		{"dataSizeOrNvid", MakePrintFunction<&Vid::dataSizeOrNvid>()},
		//{"dataFormat", MakePrintFunction<VidGraphics, &VidGraphics::dataFormat>()},
		//{"frameDuration", MakePrintFunction<VidGraphics, &VidGraphics::frameDuration>()},
		//{"numOfFrames", MakePrintFunction<VidGraphics, &VidGraphics::numOfFrames>()},
		//{"dataSize", MakePrintFunction<VidGraphics, &VidGraphics::dataSize>()},
		//{"width", MakePrintFunction<VidGraphics, &VidGraphics::width>()},
		//{"height", MakePrintFunction<VidGraphics, &VidGraphics::height>()},
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