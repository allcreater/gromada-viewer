module;
#include <imgui.h>
#include <argparse/argparse.hpp>
#include <json/json.h>

#if __INTELLISENSE__
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

#include <array>
#include <bitset>
#include <filesystem>
#include <functional>
#include <ranges>
#endif

export module application;

import std;
import sokol;
import sokol.imgui;
import imgui_utils;
import GromadaResourceReader;


struct Vid;

struct VidRawData {
    std::array<char, 34> name;
    std::uint8_t unitType;
    std::uint8_t behave;
    std::uint16_t flags;

    std::uint8_t collisionMask;
    std::uint16_t anotherWidth;
    std::uint16_t anotherHeight;
    std::uint16_t z_or_height;
    std::uint8_t maxHP;
    std::uint16_t gridRadius;
    std::uint8_t p6;
    std::uint16_t speed;

    std::uint16_t hz1;
    std::uint16_t hz2;
    std::uint8_t hz3;
    std::uint8_t army;
    std::uint8_t someWeaponIndex;
    std::uint8_t hz4;
    std::uint16_t deathSizeMargin;
    std::uint8_t somethingAboutDeath;
    std::uint8_t sX;
    std::uint8_t sY;
    std::uint8_t sZ;

    std::uint16_t hz5;
    std::uint16_t hz6;
    std::uint8_t direction;
    std::uint8_t z;

    std::array<std::uint8_t, 144> supportedActions;
    std::array<std::uint16_t, 16> children;
    std::array<std::uint8_t, 16> something;

    std::int32_t interestingNumber;

    std::uint8_t visualBehavior;
    std::uint16_t hz7;
    std::uint16_t numOfFrames;
    std::uint32_t dataSize;
    std::uint16_t imgWidth;
    std::uint16_t imgHeight;

    std::shared_ptr<Vid> vid;

    //
    void read(BinaryStreamReader reader);
    void ui() const;
    int print(std::span<char> buffer) const {
        return std::snprintf(buffer.data(), buffer.size(), "%s", name.data());
    }

    std::ostream& write_csv_line(std::ostream& stream) const {
        auto prettyName = std::string_view{ name.data(), strnlen(name.data(), name.size()) };

        stream << prettyName << ',' << +unitType << ',' << +behave << ',' << flags << ','
            << +collisionMask << ',' << anotherWidth << ',' << anotherHeight << ',' << z_or_height << ','
            << +maxHP << ',' << gridRadius << ',' << +p6 << ',' << speed << ','
            << hz1 << ',' << hz2 << ',' << +hz3 << ',' << +army << ',' << +someWeaponIndex << ',' << +hz4 << ',' << deathSizeMargin << ',' << +somethingAboutDeath << ',' << +sX << ',' << +sY << ',' << +sZ << ','
            << hz5 << ',' << hz6 << ',' << +direction << ',' << +z << ','
            //<< supportedActions << ',' << children << ',' << something << ','
            << interestingNumber;

        if (vid) {
            stream << ',' << +visualBehavior << ',' << hz7 << ',' << numOfFrames << ',' << dataSize << ',' << imgWidth << ',' << imgHeight;
        }
        else {
            stream << ",-,-,-,-,-,-";
        }

        //std::format("Hello");

        return stream << std::endl;
    }
};

struct Vid {
    std::array<std::uint8_t, 0x300> palette;
    std::vector<std::byte> data;
    std::vector<std::span<std::byte>> frames;

    void read(const VidRawData& header, BinaryStreamReader& reader);
};

#include <json/json.h>

struct DynamicObject {
    unsigned int nvid;
    int x;
    int y;
    int z;
    int direction;

    Json::Value extras;
};

struct MapHeaderRawData {
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t observerX;
    std::uint16_t observerY;
    std::uint32_t e;
    std::uint32_t f;
    std::uint32_t startTimer;
    std::uint32_t mapVersion;
};

class Map
{
public:
    Map() = default;
    explicit Map(std::span<const VidRawData> vids, GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator)
        : vids{ vids } {

		loadMapInfo(reader, resourceNavigator);
        loadDynamicObjects(reader, resourceNavigator);
    }

	void readObjectPayload(std::int16_t nvid, BinaryStreamReader& reader, DynamicObject& object) const {
		const auto readStaticObj = [&]() {
			auto unused_hp = reader.read<std::uint8_t>();
			object.extras["unused_hp"] = unused_hp;
		};

		const auto readObject2 = [&]() {
			if (m_header.mapVersion > 2) {
				auto buildTime = reader.read<std::uint8_t>();
				object.extras["buildTime"] = buildTime;
			}
			if (m_header.mapVersion > 1) {
				auto army = reader.read<std::uint8_t>();
				object.extras["army"] = army;
			}
			auto behave = reader.read<std::uint8_t>();
			object.extras["behave"] = behave;

			if (m_header.mapVersion == 0)
				return;

			for (std::int16_t itemId = 0; itemId = reader.read<std::int16_t>(), itemId >= 0; ) {
				object.extras["items"].append(itemId);
			}
		};

		switch (vids[nvid].behave) {
			// StaticObj
		case 0:
		case 1:
		case 5:
		case 6:
		case 7:
		case 8:
		case 11:
		case 14:
		case 15:
		case 16:
		case 18:
		case 20: {
			readStaticObj();
		} return;

			   // Object2
		case 2:
		case 3:
		case 4:
		case 13:
		case 17: {
			readStaticObj();
			readObject2();
		} return;

		case 9:
		case 10:
		case 12:
		case 19:
			return;

		default:
			assert(false);
			return;
		}
	}

	void loadMapInfo(GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
		auto readMapHeader = [&](BinaryStreamReader reader) {
			MapHeaderRawData result;
			reader.read_to(result.width);
			reader.read_to(result.height);
			reader.read_to(result.observerX);
			reader.read_to(result.observerY);
			reader.read_to(result.e);
			reader.read_to(result.f);
			reader.read_to(result.startTimer);
			if (reader.size() < 28) {
				result.mapVersion = 0;
				return result;
			}
			reader.read_to(result.mapVersion);

			return result;
		};

		auto it = std::ranges::find(resourceNavigator.getSections(), SectionType::MapInfo, [](const Section& section) {return section.header().type; });
		if (it == resourceNavigator.getSections().end()) {
			return;
		}

		m_header = readMapHeader(reader.beginRead(*it));
    }

	void loadDynamicObjects( GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
		auto readDynamicObjects = [&](BinaryStreamReader reader) {
			std::vector<DynamicObject> result;
            for (auto nvid = 0; nvid = reader.read<std::int16_t>(), nvid > 0; ) {
				std::array<std::uint16_t, 4> rawData;
                reader.read_to(rawData);
                
                result.push_back({
					.nvid = static_cast<unsigned int>(nvid),
					.x = rawData[0],
					.y = rawData[1],
					.z = rawData[2],
					.direction = rawData[3] //maybe it's direction + action (1 + 1 b)
                    });

                readObjectPayload(nvid, reader, result.back());
            }
			return result;
		};


		auto dynamicObjectsSection = resourceNavigator.getSections()
			| std::views::filter([](const Section& section) {return section.header().type == SectionType::Objects; })
			| std::views::transform([&](const Section& section) { return readDynamicObjects(reader.beginRead(section)); })
			| std::views::join
			| std::ranges::to<std::vector>();

		dynamicObjects = std::move(dynamicObjectsSection);
	}

	void write_json(std::ostream& stream) {
		Json::Value header;
		header["width"] = m_header.width;
		header["height"] = m_header.height;
		header["observerX"] = m_header.observerX;
		header["observerY"] = m_header.observerY;
		header["e"] = m_header.e;
		header["f"] = m_header.f;
		header["startTimer"] = m_header.startTimer;
		header["mapVersion"] = m_header.mapVersion;

		Json::Value objects;
		for (const auto& obj : dynamicObjects) {
			Json::Value object;
			object["nvid"] = obj.nvid;
			object["x"] = obj.x;
			object["y"] = obj.y;
			object["z"] = obj.z;
			object["direction"] = obj.direction;
			object["extras"] = obj.extras;

			objects.append(object);
		}

		Json::Value root;
		root["header"] = header;
		root["objects"] = objects;

		stream << root;
	}

	//void ui() {
	//	ImGui::Text("Dynamic objects: %i", dynamicObjects.size());
	//	for (const auto& obj : dynamicObjects) {
	//		ImGui::Text("NVID: %i", obj.nvid);
	//		ImGui::Text("X: %i", obj.x);
	//		ImGui::Text("Y: %i", obj.y);
	//		ImGui::Text("Z: %i", obj.z);
	//		ImGui::Text("Direction: %i", obj.direction);
	//	}


	//}

private:
    //GromadaResourceNavigator navigator;
    std::span<const VidRawData> vids;

	MapHeaderRawData m_header;
	std::vector<DynamicObject> dynamicObjects;
};

struct Resources {
    explicit Resources(std::filesystem::path path)
        : reader{ path }
        , navigator{ reader }
    {
        vids = navigator.getSections()
            | std::views::filter([](const Section& section) {return section.header().type == SectionType::Vid; })
            | std::views::transform([this](const Section& section) { VidRawData result;  result.read(reader.beginRead(section)); return result; })
            | std::ranges::to<std::vector>();

        vidsHeaders = vids
            | std::views::enumerate
            | std::views::transform([](const auto& pair) {
                static std::array<char, 256> buffer;
                
                const auto [index, data] = pair;

                auto offset = std::snprintf(buffer.data(), buffer.size(), "%i:\t", index);
                offset += data.print(std::span{ buffer.data() + offset, buffer.size() - offset });

                return std::string{ buffer.begin(), buffer.begin() + offset};
                })
            | std::ranges::to<std::vector>();
    }

    void write_csv(std::filesystem::path path) {
        std::ofstream stream{ path, std::ios_base::out /*|| std::ios_base::binary*/ };
        stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		stream << "NVID,Name,UnitType,Behave,Flags,CollisionMask,AnotherWidth,AnotherHeight,Z_or_height,MaxHP,GridRadius,P6,Speed,Hz1,Hz2,Hz3,Army,SomeWeaponIndex,Hz4,DeathSizeMargin,SomethingAboutDeath,sX,sY,sZ,Hz5,Hz6,Direction,Z,InterestingNumber,VisualBehavior,Hz7,NumOfFrames,DataSize,ImgWidth,ImgHeight" << std::endl;
		for (const auto& [index, vid] : vids | std::views::enumerate) {
			stream << index << ',';
            vid.write_csv_line(stream);
		}
    }

    GromadaResourceReader reader;
    GromadaResourceNavigator navigator;

    std::vector<VidRawData> vids;
    std::vector <std::string > vidsHeaders;
};

export class Application {
public:
    Application(const argparse::ArgumentParser& arguments)
        : resources{ arguments.get<std::filesystem::path>("res_path")}
    {
		if (auto arg = arguments.present<std::filesystem::path>("--export_csv")) {
			resources.write_csv(*arg);
		}

		if (auto arg = arguments.present<std::filesystem::path>("--map")) {
            GromadaResourceReader mapReader{*arg};
            GromadaResourceNavigator mapNavigator{ mapReader };
			map = Map{resources.vids, mapReader, mapNavigator };
		}

    }

	void on_frame() {
        ImGui::SetNextWindowPos({ 50, 100 });
        ImGui::SetNextWindowSize({ 500, 700 });
        ImGui::Begin("Header");
        ImGui::PushTabStop(true);
        bool changed = ListBox("Vids", &selected_section, std::span{ resources.vidsHeaders }, [](const auto& str) { return str.c_str(); });

        if (changed || !sectionUI) {
            sectionUI = [vid= resources.vids[selected_section]] {
                vid.ui();
            };
        }
        ImGui::PopTabStop();

        ImGui::End();


        if (sectionUI) {
            ImGui::SetNextWindowPos({ 800, 100 });
            ImGui::SetNextWindowSize({ 300, 500 });
            ImGui::Begin("Details");
            sectionUI();
            ImGui::End();
        }

        ImGui::Begin("Map");
		if (ImGui::Button("Export JSON")) {
			auto cwd = std::filesystem::current_path();
			std::ofstream stream{ "map.json", std::ios_base::out };
			map.write_json(stream);
		}
        ImGui::End();

        //ImGui::ShowDemoWindow();
	}
    
    void on_event(const sapp_event& event) {

    }

private:
    int selected_section = 0;
    Resources resources;
    Map map;
	//GromadaResourceReader resReader;
 //   GromadaResourceNavigator navigator;

    std::function<void()> sectionUI;
};

void VidRawData::read(BinaryStreamReader reader)
{
    reader.read_to(name);
    reader.read_to(unitType);
    reader.read_to(behave);
    reader.read_to(flags);

    reader.read_to(collisionMask);
    reader.read_to(anotherWidth);
    reader.read_to(anotherHeight);
    reader.read_to(z_or_height);
    reader.read_to(maxHP);
    reader.read_to(gridRadius);
    reader.read_to(p6);
    reader.read_to(speed);

    reader.read_to(hz1);
    reader.read_to(hz2);
    reader.read_to(hz3);
    reader.read_to(army);
    reader.read_to(someWeaponIndex);
    reader.read_to(hz4);
    reader.read_to(deathSizeMargin);
    reader.read_to(somethingAboutDeath);
    reader.read_to(sX);
    reader.read_to(sY);
    reader.read_to(sZ);

    reader.read_to(hz5);
    reader.read_to(hz6);
    reader.read_to(direction);
    reader.read_to(z);

    reader.read_to(supportedActions);
    reader.read_to(children);
    reader.read_to(something);

    reader.read_to(interestingNumber);

    if (interestingNumber < 0) {
        return;
    }

    reader.read_to(visualBehavior);
    reader.read_to(hz7);
    reader.read_to(numOfFrames);
    reader.read_to(dataSize);
    reader.read_to(imgWidth);
    reader.read_to(imgHeight);

    vid = std::make_shared<Vid>();
    vid->read(*this, reader);
    
}

void VidRawData::ui() const
{
    ImGui::Text("%s", name.data());
    ImGui::Text("unitType: %i ", unitType);
    ImGui::Text("Behave: %i ", behave);
    ImGui::Text("Flags % i", flags);
    ImGui::Text("Collision mask: %x", collisionMask);
    ImGui::Text("W: %i", anotherWidth);
    ImGui::Text("H: %i", anotherHeight);
    ImGui::Text("Z: %i", z_or_height);
    ImGui::Text("max HP: %i", maxHP);
    ImGui::Text("grid radius: %i", gridRadius);
    ImGui::Text("???: %i", p6);
    ImGui::Text("Speed % i", speed);

    ImGui::Text("???: %i", hz1);
    ImGui::Text("???: %i", hz2);
    ImGui::Text("???: %i", hz3);
    ImGui::Text("Army: %i", army);
    ImGui::Text("Weapon?: %i", someWeaponIndex);
    ImGui::Text("???: %i", hz4);
    ImGui::Text("Death size margin: % i", deathSizeMargin);
    ImGui::Text("Death ??: %i", somethingAboutDeath);
    ImGui::Text("some X: %i", sX);
    ImGui::Text("some Y: %i", sY);
    ImGui::Text("some Z: %i", sZ);

    ImGui::Text("???: %i", hz5);
    ImGui::Text("???: %i", hz6);
    ImGui::Text("direction? % i", direction);
    ImGui::Text("z2 : % i", z);


    ImGui::Text("interestingNumber : % i", interestingNumber);

    if (!vid) {
        return;
    }

    ImGui::Text("Visual behavior: %i", visualBehavior);
    ImGui::Text("???: %i", hz7);
    ImGui::Text("numOfFrames: %i", numOfFrames);
    ImGui::Text("dataSize: %i", dataSize);
    ImGui::Text("imgWidth: %i", imgWidth);
    ImGui::Text("imgHeight: %i", imgHeight);



    supportedActions;
    children;
    something;
}

void Vid::read(const VidRawData& header, BinaryStreamReader& reader)
{
    data.resize(header.dataSize - sizeof(palette));
    frames.resize(header.numOfFrames);

    reader.read_to(palette);
    reader.read_to(std::span{ data });

    std::byte* frameBegin = data.data();
    for (size_t i = 0; i < frames.size(); ++i) {
        //auto offset = *std::start_lifetime_as<std::uint32_t>(frameBegin);
        std::uint32_t offset;
        std::memcpy(&offset, frameBegin, sizeof offset);
        
        frameBegin += sizeof offset;
        frames[i] = { frameBegin, offset };

        frameBegin += offset;
    }
}
