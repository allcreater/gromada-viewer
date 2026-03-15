module;
#include <cstdint>
#include <cassert>

export module Gromada.GameResources;

import std;
import Gromada.ResourceReader;

export import Gromada.Resources;

export {
	class GameResources;

	// NOTE: in current implementation VidRefs are bound to the GameResources lifetime!
	class VidRef {
	private:
		const Vid* m_vid = &nullVid;
		const GameResources* m_parent = nullptr;

		static inline const Vid nullVid;

	public:
		VidRef() = default;
		VidRef(const GameResources& resources, int nvid);
		VidRef(const GameResources& resources, const Vid* vid);

		const GameResources&	parent() const;

		const Vid&				vid() const noexcept { return *m_vid; }
		std::uint16_t			nvid() const noexcept;

		operator const Vid&() const noexcept { return *m_vid; }
		operator bool () const noexcept { return m_parent; }
		const Vid* operator->() const noexcept { return m_vid; }

		auto operator<=>(const VidRef &) const = default;
	};

	class GameResources {
	public:
	    explicit GameResources(std::filesystem::path path);
		GameResources(const GameResources& ) = delete;
		GameResources& operator=(GameResources&) = delete;

		std::span<const Vid> vids() const noexcept { return m_vids; }
		std::span<const VidRef> vidRefs() const noexcept { return m_vidRefs; }
	    // unfortunately std::span yet do not have .at method
	    const Vid& getVid(int nvid) const {
	        if (nvid < 0 || nvid >= static_cast<int>(m_vids.size())) {
	            throw std::out_of_range("GameResources: nvid out of range");
	        }

	        return m_vids[nvid];
	    }

	    const std::filesystem::path& gamePath() const noexcept { return m_gamePath; }
	    std::filesystem::path mapsPath() const noexcept { return m_gamePath / "maps"; }

	    auto adjacencyData() const { return m_adjacencyData.matrix(); }
		std::span<const VidRef> baseTilesVids() const { return m_baseTilesVids; }

	private:
	    std::filesystem::path m_gamePath;
	    GromadaResourceNavigator m_navigator;

	    AdjacencyData m_adjacencyData;
	    std::vector<VidRef> m_baseTilesVids;

	    std::vector<Vid> m_vids;
		std::vector<VidRef> m_vidRefs;
	};
}
// Implementation


GameResources::GameResources(std::filesystem::path path)
	: m_gamePath(path.parent_path())
	, m_navigator{GromadaResourceReader{std::move(path)}} {

	m_navigator.visitSectionsOfType(SectionType::Vid, [this](const Section& _, BinaryStreamReader reader) { m_vids.emplace_back(reader); });

	std::ranges::for_each(m_vids, [this](Vid& vid) {
		if (const auto referenceNvid = std::get_if<std::int32_t>(&vid.graphicsData)) {
			vid.graphicsData = m_vids[*referenceNvid].graphicsData;
			if (!std::holds_alternative<Vid::Graphics>(vid.graphicsData)) {
				throw std::logic_error("GameResources: some Vids are not linked with graphics");
			}
		}
	});

	m_vidRefs = m_vids | std::views::transform([this](const Vid& vid) { return VidRef{*this, &vid}; }) | std::ranges::to<std::vector>();

	m_navigator.visitSectionsOfType(SectionType::TilesTable, [&](const Section& section, BinaryStreamReader reader) {
		m_adjacencyData = getAdjacencyData(section, reader);
		m_baseTilesVids = std::views::iota(0, std::min<int>(adjacencyData().extent(0), adjacencyData().extent(1)))
		| std::views::transform([this](int i) {
			auto nvid = std::abs(adjacencyData()[i, i]);
			return nvid ? vidRefs()[nvid] : VidRef{};
		})
		| std::ranges::to<std::vector>();
	});

	m_navigator.visitSectionsOfType(SectionType::Sound, [this](const Section& section, BinaryStreamReader reader) { getSounds(section, reader); });

	// for (const auto& [i,sound] : sounds | std::views::enumerate) {
	// 	auto data = m_reader.beginRead(sound).readAll();
	// 	std::ofstream stream{std::string("out/sound") + std::to_string(i) + ".wav", std::ios_base::out | std::ios_base::binary};
	// 	stream.write(reinterpret_cast<const char*>(data.data()), data.size());
	// }
}

VidRef::VidRef( const GameResources &resources, int nvid )
: m_vid{&resources.getVid(nvid)}
, m_parent{&resources}
{}

VidRef::VidRef(const GameResources& resources, const Vid* vid)
: m_vid{vid}
, m_parent{&resources}
{
	if (std::less<>{}(vid, resources.vids().data()) || std::greater_equal<>{}(vid, resources.vids().data() + resources.vids().size())) {
		throw std::out_of_range("VidRef must be constructed from a Vid pointer that belongs to the GameResources");
	}
}

const GameResources & VidRef::parent() const {
	if (!m_parent) [[unlikely]] {
		throw std::logic_error("VidRef is empty");
	}

	return *m_parent;
}

std::uint16_t VidRef::nvid() const noexcept {
	auto distance = std::distance(parent().vids().data(), m_vid);
	assert(distance < std::numeric_limits<std::uint16_t>::max());
	return static_cast<std::uint16_t>(distance);
}
