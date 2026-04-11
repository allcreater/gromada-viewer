module;
#include <imgui.h>
#include <argparse/argparse.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

export module application;

import std;
import imgui_sfml_adapter;
import application.model;
import application.view_model;

import Gromada.DataExporters;

// Crutch ?
export {
	namespace argparse {
		namespace details {
			template <>
			inline std::string repr<std::filesystem::path>(const std::filesystem::path& p) {
				return p.string();
			}
		} // namespace details
	} // namespace argparse
}

export class Application {
public:
	explicit Application(int argc, char* argv[])
		: m_arguments{"Gromada viewer"},
		  m_window{sf::RenderWindow( sf::VideoMode( {1280, 800} ), "Gromada viewer" )},
		  m_model{(parseArguments( argc, argv ), m_arguments.get<std::filesystem::path>( "res_path" ))},
		  m_viewModel{m_model }
	{
		ImGui::SFML::Init(m_window, false);

		if (auto arg = m_arguments.present<std::filesystem::path>("--export_csv")) {
			std::ofstream stream{*arg, std::ios_base::out};
			stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			const auto vids = m_model.get<const GameResources>().vids();
			ExportVidsToCsv(vids, stream);
		}

		if (auto arg = m_arguments.present<std::filesystem::path>("--map")) {
			m_model.loadMap(*arg);
		}

		setupFont();
	}

	void run() {
		sf::Clock deltaClock;

		while (m_window.isOpen()) {
			while (const std::optional event = m_window.pollEvent()) {
				if (event->is<sf::Event::Closed>())
					m_window.close();

				ImGui::SFML::ProcessEvent(m_window, *event);
			}
			ImGui::SFML::Update(m_window, deltaClock.restart());

			onFrame();
		}
	}

	~Application() {
		ImGui::SFML::Shutdown();
	}

private:
	void onFrame() {
		m_model.progress();
		m_viewModel.updateUI();

		m_window.clear(sf::Color{0x00, 0x80, 0xB3, 0xFF});
		ImGui::SFML::Render(m_window);
		m_window.display();

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(5ms);
	}

	void parseArguments(int argc, char* argv[]) {
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

		m_arguments.parse_args(argc, argv);
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

		ImGui::SFML::UpdateFontTexture();
	}

private:
	argparse::ArgumentParser m_arguments;
	sf::RenderWindow         m_window;
	Model                    m_model;
	ViewModel                m_viewModel;
};
