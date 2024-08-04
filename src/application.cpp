module;
#include <imgui.h>
#include <argparse/argparse.hpp>
#include <json/json.h>
#include <glm/glm.hpp>

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

constexpr ImVec2 to_imvec(const auto vec) {
	return ImVec2{ static_cast<float>(vec.x), static_cast<float>(vec.y) };
}
constexpr glm::ivec2 from_imvec(const ImVec2 vec) {
	return glm::ivec2{ static_cast<int>(vec.x), static_cast<int>(vec.y) };
}

struct Resources {
    explicit Resources(std::filesystem::path path)
        : reader{ path }
        , navigator{ reader }
    {
        vids = navigator.getSections()
            | std::views::filter([](const Section& section) {return section.header().type == SectionType::Vid; })
            | std::views::transform([this](const Section& section) { VidRawData result;  result.read(reader.beginRead(section)); return result; })
            | std::ranges::to<std::vector>();

    }

    GromadaResourceReader reader;
    GromadaResourceNavigator navigator;

    std::vector<VidRawData> vids;
};

class Model {
public:
	explicit Model(std::filesystem::path path) : m_resources{ path } { }

	const std::span<const VidRawData> vids() const {
		return m_resources.vids;
	}

	const Map& map() const {
		return m_map;
	}

    void exportMap() {
        auto cwd = std::filesystem::current_path();
        std::ofstream stream{ "map.json", std::ios_base::out };
        m_map.write_json(stream);
    }

	void loadMap(std::filesystem::path path) {
        GromadaResourceReader mapReader{ path };
        GromadaResourceNavigator mapNavigator{ mapReader };
        m_map = Map{ m_resources.vids, mapReader, mapNavigator };
	}

    void write_csv(std::filesystem::path path) {
        std::ofstream stream{ path, std::ios_base::out /*|| std::ios_base::binary*/ };
        stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        stream << "NVID,Name,UnitType,Behave,Flags,CollisionMask,AnotherWidth,AnotherHeight,Z_or_height,MaxHP,GridRadius,P6,Speed,Hz1,Hz2,Hz3,Army,SomeWeaponIndex,Hz4,DeathSizeMargin,SomethingAboutDeath,sX,sY,sZ,Hz5,Hz6,Direction,Z,InterestingNumber,VisualBehavior,Hz7,NumOfFrames,DataSize,ImgWidth,ImgHeight" << std::endl;
        for (const auto& [index, vid] : vids() | std::views::enumerate) {
            stream << index << ',';
            vid.write_csv_line(stream);
        }
    }

private:
    Resources m_resources;
    Map m_map;
};

class VidsWindowViewModel {
public:
    explicit VidsWindowViewModel(Model& model) : m_model{ model } {
        m_vidsHeaders = m_model.vids()
            | std::views::enumerate
            | std::views::transform([](const auto& pair) {
            static std::array<char, 256> buffer;

            const auto [index, data] = pair;

            auto offset = std::snprintf(buffer.data(), buffer.size(), "%i:\t", index);
            offset += data.print(std::span{ buffer.data() + offset, buffer.size() - offset });

            return std::string{ buffer.begin(), buffer.begin() + offset };
                })
            | std::ranges::to<std::vector>();
    }

    void updateUI() {
        // By default, if we don't enable ScrollX the sizing policy for each column is "Stretch"
        // All columns maintain a sizing weight, and they will occupy all available width.
        static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ContextMenuInBody;

        ImGui::Begin("Vids");
        if (ImGui::BeginTable("table1", 2, flags)) {
            ImGui::TableNextColumn();

            ImVec2 listBoxSize{ -FLT_MIN, ImGui::GetWindowHeight() - 80.0f };
            bool changed = ListBox("Vids", &m_selectedSection, std::span{ m_vidsHeaders }, [](const auto& str) { return str.c_str(); }, listBoxSize);

            if (changed || !m_sectionUI) {
                m_sectionUI = [vid = m_model.vids()[m_selectedSection]] {
                    VidRawData_ui(vid);
                    };
            }

            if (m_sectionUI) {
                ImGui::TableNextColumn();
                m_sectionUI();
            }
            ImGui::EndTable();

			if (ImGui::Button("Export CSV")) {
				m_model.write_csv("vids.csv");
			}
        }
        ImGui::End();

    }

private:
    Model& m_model;

    std::vector <std::string > m_vidsHeaders;
    std::function<void()> m_sectionUI;
    int m_selectedSection = 0;
};


class MapViewModel {
private:
    struct VidView {
        const VidRawData* pVid;

    };

public:
    explicit MapViewModel(Model& model)
        : m_model{ model }
        , m_vids{ getVids(model) }
        , m_camPos{ model.map().header().observerX, model.map().header().observerY }
    {}

	static std::vector<VidView> getVids(const Model& model) {
		return model.vids()
			| std::views::transform([](const VidRawData& vid) {
			return VidView{ &vid };
				})
			| std::ranges::to<std::vector>();
	}

    void drawMap() {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

		const auto screenSize = from_imvec(ImGui::GetMainViewport()->Size);
        const auto camOffset = m_camPos - screenSize / 2;

        const auto& map = m_model.map();
        for (const auto& obj : map.objects()) {
			const auto vid = m_vids[obj.nvid];

			const glm::ivec2 pos = glm::ivec2{ obj.x, obj.y } - camOffset;
			const glm::ivec2 size{ vid.pVid->anotherWidth, vid.pVid->anotherHeight };
            draw_list->AddRectFilled(to_imvec(pos), to_imvec(pos + size), IM_COL32(255, 0, 0, 50));
        }
		updateCameraPos();
    }

    void updateCameraPos() {
		if (ImGui::IsMouseDragging(1)) {
			m_camPos -= from_imvec(ImGui::GetIO().MouseDelta);
		}

        const auto& map = m_model.map();
        m_camPos = glm::clamp(m_camPos, glm::ivec2{ 0, 0 }, glm::ivec2{ map.header().width, map.header().height });
    }

private:
	Model& m_model;

    std::vector<VidView> m_vids;
    glm::ivec2 m_camPos;
};


class ViewModel {
public:
    explicit ViewModel(Model& model) : m_model{ model } {}

    void updateUI() {
		m_mapViewModel.drawMap();
        drawMenu();

        if (m_showVidsWindow) {
            if (!m_vidsViewModel) {
                m_vidsViewModel.emplace(m_model);
            }

            if (m_vidsViewModel) {
                m_vidsViewModel->updateUI();
            }
        }
    }


    void drawMenu()
    {
        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open")) {
                //open();
            }

            if (ImGui::MenuItem("Export map JSON")) {
				m_model.exportMap();
            }

            if (ImGui::MenuItem("Exit")) {
                sapp_request_quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Vids", "ALT+V", &m_showVidsWindow);
            
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
private:
    Model& m_model;

    bool m_showVidsWindow = false;
    std::optional<VidsWindowViewModel> m_vidsViewModel;
	MapViewModel m_mapViewModel{ m_model };
};

export class Application {
public:
    Application(const argparse::ArgumentParser& arguments)
        : m_model{ arguments.get<std::filesystem::path>("res_path")}
		, m_viewModel{ m_model }
    {
		if (auto arg = arguments.present<std::filesystem::path>("--export_csv")) {
            m_model.write_csv(*arg);
		}

		if (auto arg = arguments.present<std::filesystem::path>("--map")) {
			m_model.loadMap(*arg);
		}

    }

	void on_frame() {
		m_viewModel.updateUI();

        ImGui::ShowDemoWindow();
	}
    
    void on_event(const sapp_event& event) {

    }

private:
    Model m_model;
	ViewModel m_viewModel;
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