module;
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <imgui.h>
export module sokol.imgui;

import sokol.helpers;

export {
#include <util/sokol_imgui.h>
}

template <> struct UniqueResourceTraits<simgui_image_t> {
	using Type = simgui_image_t;
	using Desc = simgui_image_desc_t;

	static Type invalid() { return {SG_INVALID_ID}; }
	static bool is_valid(Type handle) { return handle.id != SG_INVALID_ID; }
	static Type make(Desc desc) { return simgui_make_image(desc); }
	static void destroy(Type image) { simgui_destroy_image(image); }
};

export using SimguiUniqueImage = SgUniqueResource<simgui_image_t>;


module : private;

#define SOKOL_IMPL
#include <util/sokol_imgui.h>