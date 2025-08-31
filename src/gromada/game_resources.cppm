module;
#include <cstdint>

export module Gromada.GameResources;

import std;
import Gromada.ResourceReader;

export import Gromada.Resources;

export class GameResources {
public:
    explicit GameResources(std::filesystem::path path);

    const std::span<const Vid> vids() const { return m_vids; }
    // unfortunately std::span yet do not have .at method
    const Vid& getVid(int nvid) const {
        if (nvid < 0 || nvid >= static_cast<int>(m_vids.size())) {
            throw std::out_of_range("GameResources: nvid out of range");
        }

        return m_vids[nvid];
    }

    const std::filesystem::path& gamePath() const { return m_gamePath; }
    std::filesystem::path mapsPath() const { return m_gamePath / "maps"; }

    auto adjacencyData() const { return m_adjacencyData.matrix(); }
	std::span<const int> baseTilesVids() const { return m_baseTilesVids; }

private:
    std::filesystem::path m_gamePath;
    GromadaResourceNavigator m_navigator;

    AdjacencyData m_adjacencyData;
    std::vector<int> m_baseTilesVids;

    std::vector<Vid> m_vids;
};


// Implementation


GameResources::GameResources(std::filesystem::path path)
	: m_gamePath(path.parent_path()), m_navigator{GromadaResourceReader{std::move(path)}} {

	m_navigator.visitSectionsOfType(SectionType::Vid, [this](const Section& _, BinaryStreamReader reader) { m_vids.emplace_back(reader); });

	std::ranges::for_each(m_vids, [this](Vid& vid) {
		if (const auto referenceNvid = std::get_if<std::int32_t>(&vid.graphicsData)) {
			vid.graphicsData = m_vids[*referenceNvid].graphicsData;
			if (!std::holds_alternative<Vid::Graphics>(vid.graphicsData)) {
				throw std::logic_error("GameResources: some Vids are not linked with graphics");
			}
		}
	});

	m_navigator.visitSectionsOfType(SectionType::TilesTable, [&](const Section& section, BinaryStreamReader reader) {
		m_adjacencyData = getAdjacencyData(section, reader);
		m_baseTilesVids = std::views::iota(0, std::min<int>(adjacencyData().extent(0), adjacencyData().extent(1))) |
						  std::views::transform([this](int i) { return std::abs(adjacencyData()[i, i]); }) | std::ranges::to<std::vector>();
	});

	m_navigator.visitSectionsOfType(SectionType::Sound, [this](const Section& section, BinaryStreamReader reader) { getSounds(section, reader); });

	// for (const auto& [i,sound] : sounds | std::views::enumerate) {
	// 	auto data = m_reader.beginRead(sound).readAll();
	// 	std::ofstream stream{std::string("out/sound") + std::to_string(i) + ".wav", std::ios_base::out | std::ios_base::binary};
	// 	stream.write(reinterpret_cast<const char*>(data.data()), data.size());
	// }
}