module;
#include <imgui.h>
#include <argparse/argparse.hpp>

#include <glm/glm.hpp>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>


#if __INTELLISENSE__
#include <array>
#include <bitset>
#include <filesystem>
#include <functional>
#include <ranges>
#endif

export module application;

import std;

import application.model;
import application.view_model;

import Gromada.DataExporters;

export class Application {
public:
    Application(const argparse::ArgumentParser& arguments)
        : m_model{ arguments.get<std::filesystem::path>("res_path")}
		, m_viewModel{ m_model }
    {
		if (auto arg = arguments.present<std::filesystem::path>("--export_csv")) {
			std::ofstream stream{*arg, std::ios_base::out /*|| std::ios_base::binary*/};
			stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			const auto vids = m_model.get<const GameResources>()->vids();
			ExportVidsToCsv(vids, stream);
		}

		if (auto arg = arguments.present<std::filesystem::path>("--map")) {
			m_model.loadMap(*arg);
		}

		setupFont();
    }

	void on_frame() {
		m_model.progress();
		m_viewModel.updateUI();

        //ImGui::ShowDemoWindow();
	}

    void on_event(const sapp_event& event) {
		simgui_handle_event(&event);
    }

	static void setupFont() {
		const auto findFontInDirectory = [&](const std::filesystem::path& path) -> std::optional<std::filesystem::path> {
			using namespace std::filesystem;
			auto ttf_view = std::views::all(directory_iterator{path}) |
							std::views::filter([](const directory_entry& entry) { return entry.is_regular_file() && entry.path().extension() == ".ttf"; });
			for (auto&& entry : ttf_view) {
				return std::move(entry.path());
			};

		    return std::nullopt;
		};

        std::filesystem::path fontPath = findFontInDirectory(std::filesystem::current_path()).value_or("C://Windows//Fonts//segoeui.ttf");
        if (!std::filesystem::exists(fontPath))
            return;

		ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImFontConfig cfg;
		cfg.SizePixels = 17.0f * io.DisplayFramebufferScale.x;
		//cfg.OversampleH = cfg.OversampleV = 3;
		cfg.RasterizerMultiply = 1.3f;
		cfg.PixelSnapH = true;
		cfg.GlyphRanges = io.Fonts->GetGlyphRangesCyrillic();

		io.FontGlobalScale = 1.0f;
		io.Fonts->Clear();
		io.Fonts->AddFontFromFileTTF(fontPath.generic_string().c_str(), 0.0f, &cfg, nullptr);
		io.Fonts->Build();

		simgui_destroy_fonts_texture();
		simgui_create_fonts_texture(simgui_font_tex_desc_t{});
	}

private:
    Model m_model;
	ViewModel m_viewModel;
};
