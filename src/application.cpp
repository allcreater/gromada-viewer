module;
#include <imgui.h>
#include <argparse/argparse.hpp>

#include <glm/glm.hpp>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application;

import std;

import application.model;
import application.view_model;

import Gromada.DataExporters;

template <>
inline std::string argparse::details::repr<std::filesystem::path>(const std::filesystem::path& p) {
	return p.string();
}

export class Application {
public:
    Application(const std::vector<std::string>& args)
	    : m_arguments{"Gromada viewer"}
        , m_model{ (parseArguments( args ), m_arguments.get<std::filesystem::path>( "res_path" ))}
		, m_viewModel{ m_model }
    {
		if (auto arg = m_arguments.present<std::filesystem::path>("--export_csv")) {
			std::ofstream stream{*arg, std::ios_base::out /*|| std::ios_base::binary*/};
			stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			const auto vids = m_model.get<const GameResources>().vids();
			ExportVidsToCsv(vids, stream);
		}

		if (auto arg = m_arguments.present<std::filesystem::path>("--map")) {
			m_model.loadMap(*arg);
		}

		setupFont();
    }

	void on_frame() {
		m_model.progress();
		m_viewModel.updateUI();

        //ImGui::ShowDemoWindow();

    	// the sokol_gfx draw pass
    	sg_pass pass = {};
    	pass.action = {
    		.colors = {
				{.load_action = SG_LOADACTION_CLEAR, .clear_value = {0.0f, 0.5f, 0.7f, 1.0f}},
			},
    	};
    	pass.swapchain = sglue_swapchain();
    	sg_begin_pass(&pass);
    	simgui_render();
    	sg_end_pass();
    	sg_commit();

    	using namespace std::chrono_literals;
    	std::this_thread::sleep_for(5ms);
	}

    void on_event(const sapp_event& event) {
		simgui_handle_event(&event);
    }

private:
	struct SokolHolder {
		SokolHolder() {
			sg_setup({
				.image_pool_size = 1024,
				.view_pool_size = 1024,
				.logger = {.func = slog_func},
				.environment = sglue_environment(),
			});

			// use sokol-imgui with all default-options (we're not doing
			// multi-sampled rendering or using non-default pixel formats)
			simgui_setup({
				//.no_default_font = true,
				.logger = {.func = slog_func},
			});
		}

		SokolHolder(const SokolHolder&) = delete;
		SokolHolder& operator=(const SokolHolder&) = delete;

		~SokolHolder() {
			simgui_shutdown();
			sg_shutdown();
		}
	};

	void parseArguments(const std::vector<std::string>& args) {
    	auto to_writtable_path = [](auto&& str) {
    		return std::filesystem::path{std::forward<decltype(str)>(str)};
    	};

    	auto to_readable_path = [](auto&& str) {
    		std::filesystem::path result{std::forward<decltype(str)>(str)};

    		if (!exists(result)) {
    			throw std::runtime_error("Path does not exist: " + result.string());
    		}

    		return result;
    	};

    	m_arguments.add_argument("res_path")
			.default_value(std::filesystem::current_path() / "fw.res")
			.action(to_readable_path)
			.implicit_value(true)
			.remaining();

    	m_arguments.add_argument("--export_csv")
			.action(to_writtable_path)
			.help("Export CSV file with vids data");

    	m_arguments.add_argument("--map")
			.action(to_readable_path)
			.help("a path to a .map file");

    	m_arguments.parse_args(args);
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
	}

private:
	argparse::ArgumentParser m_arguments;
	Model                    m_model;
	SokolHolder			     m_sokolHolder;
	ViewModel                m_viewModel;
};
