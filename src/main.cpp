
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <util/sokol_imgui.h>

#include <cassert>

import application;
import std;

class SappWrapper {
	std::unique_ptr<Application> app;
	std::vector<std::string> args;

	SappWrapper(int argc, char* argv[]) {
		std::copy(argv, argv + argc, std::back_inserter(args));
	}

	void init() {
		try {
			app = std::make_unique<Application>(args);
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			sapp_quit();
		}
	}

	void frame() {
		if (!app)
			return;

		const int width = sapp_width();
		const int height = sapp_height();
		simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

		app->on_frame();

	}

	void cleanup() {
		delete this;
	}

	void input(const sapp_event* event) {
		assert(event);
		app->on_event(*event);
	}

	template <auto MemberFn, typename... Args>
	static void bind(Args... args, void* userdata) {
		auto* self = static_cast<SappWrapper*>(userdata);
		std::invoke(MemberFn, self, args...);
	};

public:
	static sapp_desc create(int argc, char* argv[]) {
		return {
			.user_data = new SappWrapper{argc, argv},
			.init_userdata_cb = bind<&SappWrapper::init>,
			.frame_userdata_cb = bind<&SappWrapper::frame>,
			.cleanup_userdata_cb = bind<&SappWrapper::cleanup>,
			.event_userdata_cb = bind<&SappWrapper::input, const sapp_event*>,
			.window_title = "Gromada viewer",
			.enable_clipboard = true,
		};
	}
};

sapp_desc sokol_main(int argc, char* argv[]) {
	return SappWrapper::create(argc, argv);
}
