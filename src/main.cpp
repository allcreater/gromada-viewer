#include <argparse/argparse.hpp>

#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

#if __INTELLISENSE__
#endif


import application;

static sg_pass_action pass_action;

#include <cassert>


import std;

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
	}

	void init() {
		// setup sokol-gfx, sokol-time and sokol-imgui
		sg_desc desc = {
			.image_pool_size = 1024,
		};
		desc.environment = sglue_environment();
		desc.logger.func = slog_func;
		sg_setup(&desc);

		// use sokol-imgui with all default-options (we're not doing
		// multi-sampled rendering or using non-default pixel formats)
		simgui_desc_t simgui_desc = { };
		simgui_desc.logger.func = slog_func;
		simgui_setup(&simgui_desc);

		// initial clear color
		pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		pass_action.colors[0].clear_value = {0.0f, 0.5f, 0.7f, 1.0f};

		app = std::make_unique<Application>(arguments);
	}

	void frame() {
		const int width = sapp_width();
		const int height = sapp_height();
		simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

		app->on_frame();

		// the sokol_gfx draw pass
		sg_pass pass = {};
		pass.action = pass_action;
		pass.swapchain = sglue_swapchain();
		sg_begin_pass(&pass);
		simgui_render();
		sg_end_pass();
		sg_commit();

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(5ms);
	}

	void cleanup() {
		app.reset();

		simgui_shutdown();
		sg_shutdown();

		delete this;
	}

	void input(const sapp_event* event) {
		assert(event);
		app->on_event(*event);

		simgui_handle_event(event);
	}

	template <auto MemberFn, typename... Args>
	static void bind(Args... args, void* userdata) {
		auto* self = reinterpret_cast<App*>(userdata);
		std::invoke(MemberFn, self, args...);
	};

public:
	static sapp_desc create(int argc, char* argv[]) {
		return {
			.user_data = new App{argc, argv},
			.init_userdata_cb = bind<&App::init>,
			.frame_userdata_cb = bind<&App::frame>,
			.cleanup_userdata_cb = bind<&App::cleanup>,
			.event_userdata_cb = bind<&App::input, const sapp_event*>,
			.window_title = "Gromada viewer",
			.enable_clipboard = true,
		};
	}
};

sapp_desc sokol_main(int argc, char* argv[]) {
	return App::create(argc, argv);
}
