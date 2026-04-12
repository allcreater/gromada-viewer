module;
#include <glm/glm.hpp>
#include <SFML/Graphics.hpp>

export module framebuffer;

import std;
import Gromada.Resources;
import Gromada.SoftwareRenderer;

export class Framebuffer : sf::Texture {
public:
	Framebuffer() = default;
	Framebuffer(int width, int height)
		: sf::Texture{sf::Vector2u{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)}, false},
		  m_data{static_cast<size_t>(width * height), RGBA8{0, 0, 0, 0}},
		  m_dataDesc{m_data.data(), std::dextents<int, 2>{height, width}} {}

	void resize(glm::ivec2 newSize) {
		if (m_dataDesc.extent(0) == newSize.y || m_dataDesc.extent(1) == newSize.x || newSize.x <= 0 || newSize.y <= 0)
			return;

		Framebuffer copy{newSize.x, newSize.y};
		std::swap(*this, copy);
	}

	void clear(RGBA8 color) { std::ranges::fill(m_data, color); }

	void commitToGpu() {
		update(reinterpret_cast<const std::uint8_t*>(m_data.data()));
	}

    [[nodiscard]] const sf::Texture& getImage() & { return *this; }
    [[nodiscard]] sf::Texture&& getImage() && { return std::move(*this); }

	operator FramebufferRef() { return m_dataDesc; }

private:
	std::vector<RGBA8> m_data;
	FramebufferRef m_dataDesc;
};
