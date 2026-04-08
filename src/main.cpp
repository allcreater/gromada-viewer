#include <argparse/argparse.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <cassert>

import application;
import std;
import imgui_sfml_adapter;


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


class App {
	std::unique_ptr<Application> app;
	argparse::ArgumentParser arguments;
	sf::RenderWindow window;

public:
	App(int argc, char* argv[])
		: arguments{"Gromada viewer"}
	{
		arguments.add_argument("res_path")
			.default_value(std::filesystem::current_path() / "fw.res")
			.action(to_readable_path)
			.implicit_value(true)
			.remaining();

		arguments.add_argument("--export_csv")
			.action(to_writtable_path)
			//.default_value(std::filesystem::current_path() / "vids.csv")
			//.implicit_value(true)
			.help("Export CSV file with vids data");

		arguments.add_argument("--map")
			.action(to_readable_path)
			.help("a path to a .map file");

		arguments.parse_args(argc, argv);

		window = sf::RenderWindow(sf::VideoMode({1280, 800}), "Gromada viewer");

		ImGui::SFML::Init( window, true );
		app = std::make_unique<Application>(arguments);
	}

	~App() {
		ImGui::SFML::Shutdown();
	}

	void run() {
		sf::Clock deltaClock;

		while ( window.isOpen() )
		{
			while ( const std::optional event = window.pollEvent() )
			{
				if ( event->is<sf::Event::Closed>() )
					window.close();

				ImGui::SFML::ProcessEvent(window, *event);
			}

			ImGui::SFML::Update(window, deltaClock.restart());

			frame();

			window.clear(sf::Color{0x00, 0x80, 0xB3, 0xFF});
			ImGui::SFML::Render(window);
			window.display();
		}
	}

	void frame() {
		if (!app)
			return;

		app->on_frame();

		ImGui::SFML::Render(window);

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(5ms);
	}


};

int main(int argc, char* argv[]) {
	App app{argc, argv};
	app.run();
}
