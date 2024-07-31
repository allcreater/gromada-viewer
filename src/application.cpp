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

import Gromada.ResourceReader;
import Gromada.Resources;

void VidRawData_ui(const VidRawData& self);

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
				VidRawData_ui(vid);
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

void VidRawData_ui(const VidRawData& self)
{
    ImGui::Text("%s", self.name.data());
    ImGui::Text("unitType: %i ", self.unitType);
    ImGui::Text("Behave: %i ", self.behave);
    ImGui::Text("Flags % i", self.flags);
    ImGui::Text("Collision mask: %x", self.collisionMask);
    ImGui::Text("W: %i", self.anotherWidth);
    ImGui::Text("H: %i", self.anotherHeight);
    ImGui::Text("Z: %i", self.z_or_height);
    ImGui::Text("max HP: %i", self.maxHP);
    ImGui::Text("grid radius: %i", self.gridRadius);
    ImGui::Text("???: %i", self.p6);
    ImGui::Text("Speed % i", self.speed);

    ImGui::Text("???: %i", self.hz1);
    ImGui::Text("???: %i", self.hz2);
    ImGui::Text("???: %i", self.hz3);
    ImGui::Text("Army: %i", self.army);
    ImGui::Text("Weapon?: %i", self.someWeaponIndex);
    ImGui::Text("???: %i", self.hz4);
    ImGui::Text("Death size margin: % i", self.deathSizeMargin);
    ImGui::Text("Death ??: %i", self.somethingAboutDeath);
    ImGui::Text("some X: %i", self.sX);
    ImGui::Text("some Y: %i", self.sY);
    ImGui::Text("some Z: %i", self.sZ);

    ImGui::Text("???: %i", self.hz5);
    ImGui::Text("???: %i", self.hz6);
    ImGui::Text("direction? % i", self.direction);
    ImGui::Text("z2 : % i", self.z);


    ImGui::Text("interestingNumber : % i", self.interestingNumber);

    if (!self.vid) {
        return;
    }

    ImGui::Text("Visual behavior: %i", self.visualBehavior);
    ImGui::Text("???: %i", self.hz7);
    ImGui::Text("numOfFrames: %i", self.numOfFrames);
    ImGui::Text("dataSize: %i", self.dataSize);
    ImGui::Text("imgWidth: %i", self.imgWidth);
    ImGui::Text("imgHeight: %i", self.imgHeight);
}