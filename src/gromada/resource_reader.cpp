module;
#if __INTELLISENSE__
#include <array>
#include <fstream>
#include <span>
#include <vector>
#include <optional>
#include <concepts>
#include <ranges>
#endif


export module Gromada.ResourceReader;
import std;

export {

	class BinaryStreamReader {
	public:
		explicit BinaryStreamReader(std::istream& stream, std::size_t length) : m_stream{ stream }, m_dataLength{ length } {}
		explicit BinaryStreamReader(std::istream& stream) : m_stream{ stream }, m_dataLength{ std::numeric_limits<std::size_t>::max() } {}

		void read_to(std::span<std::byte> out) {
			if (m_count + out.size() > m_dataLength)
				throw std::overflow_error("out of section access");

			m_stream.read(reinterpret_cast<char*>(out.data()), out.size());

			m_count += out.size();
		}

		template <typename T>
		void read_to(T& out) {
			read_to(std::span{ reinterpret_cast<std::byte*>(&out), sizeof (T) });
		}

		template <typename T>
		T read() {
			T result;
			read_to(result);
			return result;
		}

		void skip(std::size_t bytes) { 
			if (m_count + bytes > m_dataLength)
				throw std::overflow_error("");

			m_stream.seekg(bytes, std::ios_base::cur); 
			m_count += bytes;
		}

		std::vector<std::byte> readAll() && {
			std::vector<std::byte> result(m_dataLength - m_count);
			read_to(std::span{result});
			return result;
		}

		std::size_t size() const noexcept { return m_dataLength; }
		std::streampos tellg() const noexcept { return m_stream.tellg(); }
		//std::istream& stream() const&& noexcept { return m_stream; }

	private:
		std::istream& m_stream;
		std::size_t m_dataLength;
		std::size_t m_count = 0;
	};



	enum class SectionType : std::uint8_t {
		None = 0,
		MapInfo = 1,
		Objects = 2,
		Command = 4,
		ObjectsIds = 5,
		Army = 6,
		Vid = '!',
		Sound = '\"',
		Weapon = '#',
		TilesTable = '%',
	};

	struct SectionHeader {
		SectionType type = SectionType::None;
		std::uint32_t nextSectionOffset;
		std::uint32_t elementCount;
		std::uint16_t dataOffset;

		static SectionHeader read(std::istream& stream) {
			SectionHeader sectionHeader;
			stream.read(reinterpret_cast<char*>(&sectionHeader.type), 1);
			stream.read(reinterpret_cast<char*>(&sectionHeader.nextSectionOffset), 4);
			stream.read(reinterpret_cast<char*>(&sectionHeader.elementCount), 4);
			stream.read(reinterpret_cast<char*>(&sectionHeader.dataOffset), 2);

			return sectionHeader;
		}
	};

	class StreamSpan {
	public:
		StreamSpan(std::streampos beginPos, std::streampos endPos)
			: m_beginPos{beginPos}, m_endPos{endPos} {}
		StreamSpan(std::streampos beginPos, std::streamoff offset)
			: StreamSpan{beginPos, beginPos + offset} {}

		BinaryStreamReader beginRead(std::istream& stream) const noexcept {
			stream.seekg(m_beginPos, std::ios_base::beg);
			return BinaryStreamReader{stream, static_cast<std::size_t>(m_endPos - m_beginPos)};
		}

	private:
		std::streampos m_beginPos, m_endPos;
	};

	class Section : public StreamSpan {
	public:
		Section(const SectionHeader& header, std::streampos beginPos) 
			: StreamSpan{beginPos + static_cast<std::streampos>(11 + header.dataOffset), beginPos + static_cast<std::streamoff>(5 + header.nextSectionOffset)}
			, m_header{ header }
			, m_beginPos{ beginPos } {}

		const SectionHeader& header() const noexcept { return m_header; }

	private:
		SectionHeader m_header;
		std::streampos m_beginPos;
	};

	class GromadaResourceReader {
	public:
		explicit GromadaResourceReader(std::filesystem::path path) : m_stream{ path, std::ios_base::in | std::ios_base::binary } {
			m_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			m_stream.read(reinterpret_cast<char*>(&m_sectionsCount), sizeof(std::uint32_t));
			m_currentSectionBegin = m_stream.tellg();
		}

		void goStart() {
			m_stream.seekg(4, std::ios_base::beg);
			m_currentSectionBegin = m_stream.tellg();
			m_currentSectionIndex = 0;
		}

		std::optional<Section> nextSection() {
			m_stream.seekg(m_currentSectionBegin, std::ios_base::beg);
			auto currentSection = SectionHeader::read(m_stream);

			// skip unused bytes
			m_stream.seekg(currentSection.dataOffset, std::ios_base::cur);

			if (m_currentSectionIndex++ == m_sectionsCount || currentSection.nextSectionOffset == 0) {
				return std::nullopt;
			}

			return std::optional<Section>{
				std::in_place_t{},
				currentSection,
				std::exchange(m_currentSectionBegin, m_currentSectionBegin + static_cast<std::streamoff>(5 + currentSection.nextSectionOffset)),
			};
		}

		BinaryStreamReader beginRead(const StreamSpan& section) {
			return section.beginRead(m_stream);
		}

		//void borrowStream(std::function<void(std::istream&)> func) {
		//	auto pos = m_stream.tellg();
		//	func(m_stream);
		//	m_stream.seekg(pos);
		//}

		std::uint32_t getNumSections() const { return m_sectionsCount;  }

	private:
		std::ifstream m_stream;
		std::uint32_t m_sectionsCount = 0;
		std::uint32_t m_currentSectionIndex = 0;

		std::streampos m_currentSectionBegin;
	};


	class GromadaResourceNavigator {
	public:
		GromadaResourceNavigator() = default;
		GromadaResourceNavigator(GromadaResourceReader& reader) {
			reader.goStart();

			m_sections.reserve(reader.getNumSections());
			for (int i = 0; i < reader.getNumSections(); ++i) {
				auto section = reader.nextSection();
				if (!section)
					break;

				m_sections.push_back(std::move(*section));
			}
		}

		std::span<const Section> getSections() const { return m_sections; }

	private:
		std::vector<Section> m_sections;
	};

}
