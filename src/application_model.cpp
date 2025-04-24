module;

export module application.model;

import std;
import utils;

export import Gromada.ResourceReader;
export import Gromada.Resources;

export import engine.objects_view;


struct Resources {
	explicit Resources(std::filesystem::path path)
		: reader{path}, navigator{reader} {
		// clang-format off
        vids = navigator.getSections()
            | std::views::filter([](const Section& section) { return section.header().type == SectionType::Vid; })
            | std::views::transform([this](const Section& section) { Vid result;  result.read(reader.beginRead(section)); return result; })
            | std::ranges::to<std::vector>();
		// clang-format on

		auto sounds = navigator.getSections() 
			| std::views::filter([](const Section& section) { return section.header().type == SectionType::Sound; }) 
			| std::views::transform(std::bind_front(getSounds, std::ref(reader)))
			| std::views::join
			| std::ranges::to<std::vector>();

		//for (const auto& [i,sound] : sounds | std::views::enumerate) {
		//	auto data = reader.beginRead(sound).readAll();
		//	std::ofstream stream{std::string("out/sound") + std::to_string(i) + ".wav", std::ios_base::out | std::ios_base::binary};
		//	stream.write(reinterpret_cast<const char*>(data.data()), data.size());
		//}
	}


	GromadaResourceReader reader;
	GromadaResourceNavigator navigator;

	std::vector<Vid> vids;
};

//TODO: move
export struct ObjectToPlaceMessage {
	unsigned int nvid;
	std::uint8_t direction;
};

export class Model {
public:
	explicit Model(std::filesystem::path path)
		: m_resources{path}, m_gamePath{path.parent_path()}, m_objectsView{m_resources.vids, m_activeMap} {}

	const std::span<const Vid> vids() const { return m_resources.vids; }
	
	// NOTE: Map is just a DTO type, for game logic a new class will be needed
	Map& map() { return m_activeMap; }
	const Map& map() const { return m_activeMap; }
	const std::filesystem::path& activeMapPath() const { return m_activeMapPath; }

	const ObjectsView& objectsView() const { return m_objectsView; }

	const std::filesystem::path& gamePath() const { return m_gamePath; }

	std::vector<std::size_t>& selectedMapObjects() { return m_selectedObjects;  }

	void update() { m_objectsView.update(); }

	void loadMap(const std::filesystem::path& path) {
		GromadaResourceReader mapReader{path};
		GromadaResourceNavigator mapNavigator{mapReader};

		m_activeMap = Map::load(m_resources.vids, mapReader, mapNavigator);
		m_activeMapPath = std::move(path);
		m_selectedObjects.clear();
	}

private:
	Resources m_resources;
	Map m_activeMap;
	ObjectsView m_objectsView;
	std::vector<std::size_t> m_selectedObjects;

	std::filesystem::path m_activeMapPath;
	std::filesystem::path m_gamePath;
};