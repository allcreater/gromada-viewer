module;
export module sokol;

import sokol.helpers;

export {
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
}



template <> struct UniqueResourceTraits<sg_image> {
	using Type = sg_image;
	using Desc = sg_image_desc;

	static Type invalid() { return {SG_INVALID_ID}; }
	static bool is_valid(Type handle){return handle.id != SG_INVALID_ID; }
	static Type make(Desc desc) { return sg_make_image(desc); }
	static void destroy(Type image) { sg_destroy_image(image); }
};

template <> struct UniqueResourceTraits<sg_sampler> {
	using Type = sg_sampler;
	using Desc = sg_sampler_desc;

	static Type invalid() { return {SG_INVALID_ID}; }
	static bool is_valid(Type handle){return handle.id != SG_INVALID_ID; }
	static Type make(Desc desc) { return sg_make_sampler(desc); }
	static void destroy(Type image) { sg_destroy_sampler(image); }
};

export using SgUniqueImage = SgUniqueResource<sg_image>;
export using SgUniqueSampler = SgUniqueResource<sg_sampler>;



module : private;

#define SOKOL_IMPL
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>