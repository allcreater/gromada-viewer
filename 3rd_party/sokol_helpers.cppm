module;
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>

export module sokol.helpers;

import std;

export template <typename T> struct UniqueResourceTraits;

export template <typename T>
// requires requires { UniqueResourceTraits<T>::Type; }
class SgUniqueResource {
public:
	using NativeType = typename UniqueResourceTraits<T>::Type;
	using DescType = typename UniqueResourceTraits<T>::Desc;

	SgUniqueResource() = default;
	explicit SgUniqueResource(NativeType resource)
		: m_resource{resource} {}
	SgUniqueResource(DescType desc)
		: m_resource{UniqueResourceTraits<T>::make(desc)} {
		if (!UniqueResourceTraits<T>::is_valid(m_resource))
			throw std::runtime_error("Failed to create resource");
	}
	SgUniqueResource(const SgUniqueResource&) = delete;
	SgUniqueResource& operator=(const SgUniqueResource&) = delete;
	SgUniqueResource(SgUniqueResource&& rhv) noexcept
		: m_resource{std::exchange(rhv.m_resource, UniqueResourceTraits<T>::invalid())} {}
	SgUniqueResource& operator=(SgUniqueResource&& rhv) noexcept {
		SgUniqueResource temp{std::move(rhv)};
		std::swap(m_resource, temp.m_resource);
		return *this;
	}
	~SgUniqueResource() {
		if (UniqueResourceTraits<T>::is_valid(m_resource)) {
			UniqueResourceTraits<T>::destroy(m_resource);
		}
	}

	operator NativeType() const noexcept { return m_resource; }

private:
	NativeType m_resource = UniqueResourceTraits<T>::invalid();
};

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
