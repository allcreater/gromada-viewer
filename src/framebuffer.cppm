module;
#include <glm/glm.hpp>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>

export module framebuffer;

import std;
import sokol.helpers;

import Gromada.Resources;
import Gromada.SoftwareRenderer;

export class Framebuffer {
public:
	Framebuffer() = default;
	Framebuffer(int width, int height)
		: m_image{sg_image_desc{
			  .type = SG_IMAGETYPE_2D,
			  .width = width,
			  .height = height,
			  .usage = SG_USAGE_STREAM,
			  .pixel_format = SG_PIXELFORMAT_RGBA8,
		  }},
		  m_data{static_cast<size_t>(width * height), RGBA8{0, 0, 0, 0}},
		  m_dataDesc{m_data.data(), std::dextents<int, 2>{height, width}} {}

	void resize(glm::ivec2 newSize) {
		if (m_dataDesc.extent(0) == newSize.y || m_dataDesc.extent(1) == newSize.x)
			return;

		Framebuffer copy{newSize.x, newSize.y};
		std::swap(*this, copy);
	}

	void clear(RGBA8 color) { std::ranges::fill(m_data, color); }

	void commitToGpu() { sg_update_image(m_image, sg_image_data{{{{.ptr = m_data.data(), .size = m_data.size() * sizeof(RGBA8)}}}}); }

    [[nodiscard]]  sg_image getImage() const { return m_image; }

	operator FramebufferRef() { return m_dataDesc; }

private:
	SgUniqueImage m_image;
	std::vector<RGBA8> m_data;
	FramebufferRef m_dataDesc;
};
