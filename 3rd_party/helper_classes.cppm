export module sokol.helpers;

import std;

export template <typename T> struct UniqueResourceTraits;

export template <typename T>
	//requires requires { UniqueResourceTraits<T>::Type; }
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
		std::swap(*this, rhv);
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