module;
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <imgui.h>
export module sokol.imgui;

export {
#include <util/sokol_imgui.h>
}

module : private;

#define SOKOL_IMPL
#include <util/sokol_imgui.h>