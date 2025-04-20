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
			ExportVidsToCsv(m_model.vids(), stream);
		}

		if (auto arg = arguments.present<std::filesystem::path>("--map")) {
			m_model.loadMap(*arg);
		}

    }

	void on_frame() {
		m_model.update();
		m_viewModel.updateUI();

        //ImGui::ShowDemoWindow();
	}
    
    void on_event(const sapp_event& event) {

    }

private:
    Model m_model;
	ViewModel m_viewModel;
};
