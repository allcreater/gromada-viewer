export module Gromada.GameResources;

import std;
import Gromada.ResourceReader;

export import Gromada.Resources;

export class GameResources {
public:
    explicit GameResources(std::filesystem::path path)
        : m_reader{path}, m_navigator{m_reader} {
        // clang-format off
        m_vids = m_navigator.getSections()
            | std::views::filter([](const Section& section) { return section.header().type == SectionType::Vid; })
            | std::views::transform([this](const Section& section) { Vid result;  result.read(m_reader.beginRead(section)); return result; })
            | std::ranges::to<std::vector>();
        // clang-format on

        std::ranges::for_each(m_vids, [this](Vid& vid) {
            if (const auto referenceNvid = std::get_if<std::int32_t>(&vid.graphicsData)) {
                vid.graphicsData = m_vids[*referenceNvid].graphicsData;
                if (!std::holds_alternative<Vid::Graphics>(vid.graphicsData)) {
                    throw std::logic_error("GameResources: some Vids are not linked with graphics");
                }
            }
        } );

        auto sounds = m_navigator.getSections()
            | std::views::filter([](const Section& section) { return section.header().type == SectionType::Sound; })
            | std::views::transform(std::bind_front(getSounds, std::ref(m_reader)))
            | std::views::join
            | std::ranges::to<std::vector>();

        // for (const auto& [i,sound] : sounds | std::views::enumerate) {
        // 	auto data = m_reader.beginRead(sound).readAll();
        // 	std::ofstream stream{std::string("out/sound") + std::to_string(i) + ".wav", std::ios_base::out | std::ios_base::binary};
        // 	stream.write(reinterpret_cast<const char*>(data.data()), data.size());
        // }
    }

    const std::span<const Vid> vids() const { return m_vids; }

    std::tuple<const Vid&, const VidGraphics&> getVid(std::size_t index) const {
        return {m_vids[index], *std::get<Vid::Graphics>(m_vids[index].graphicsData)};
    }
private:
    GromadaResourceReader m_reader;
    GromadaResourceNavigator m_navigator;

    std::vector<Vid> m_vids;
};