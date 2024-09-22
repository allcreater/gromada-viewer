module;

export module application.model;

import std;

export import Gromada.ResourceReader;
export import Gromada.Resources;

struct Resources {
	explicit Resources(std::filesystem::path path)
		: reader{path}, navigator{reader} {
		// clang-format off
        vids = navigator.getSections()
            | std::views::filter([](const Section& section) { return section.header().type == SectionType::Vid; })
            | std::views::transform([this](const Section& section) { VidRawData result;  result.read(reader.beginRead(section)); return result; })
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

	std::vector<VidRawData> vids;
};

export class Model {
public:
	explicit Model(std::filesystem::path path)
		: m_resources{path}, m_gamePath{path.parent_path()} {}

	const std::span<const VidRawData> vids() const { return m_resources.vids; }

	const Map& map() const { return m_map; }

	const std::filesystem::path& gamePath() const { return m_gamePath; }

	void exportMap(const std::filesystem::path& path) {
		std::ofstream stream{path, std::ios_base::out};
		m_map.write_json(stream);
	}

	void loadMap(const std::filesystem::path& path) {
		GromadaResourceReader mapReader{path};
		GromadaResourceNavigator mapNavigator{mapReader};
		m_map = Map{m_resources.vids, mapReader, mapNavigator};
		m_map.filename() = std::move(path);
	}

	void write_csv(const std::filesystem::path& path) {
		std::ofstream stream{path, std::ios_base::out /*|| std::ios_base::binary*/};
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		stream << "NVID,Name,UnitType,Behave,Flags,CollisionMask,AnotherWidth,AnotherHeight,Z_or_height,MaxHP,GridRadius,P6,Speed,Hz1,Hz2,Hz3,Army,"
				  "SomeWeaponIndex,Hz4,DeathSizeMargin,SomethingAboutDeath,sX,sY,sZ,Hz5,Hz6,Direction,Z,InterestingNumber,VisualBehavior,Hz7,NumOfFrames,"
				  "DataSize,ImgWidth,ImgHeight"
			   << std::endl;
		for (const auto& [index, vid] : vids() | std::views::enumerate) {
			stream << index << ',';
			vid.write_csv_line(stream);
		}
	}

private:
	Resources m_resources;
	Map m_map;
	std::filesystem::path m_gamePath;
};