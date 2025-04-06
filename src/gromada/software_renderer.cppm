module;
#if __INTELLISENSE__
#include <array>
#include <fstream>
#include <span>
#include <mdspan>
#include <vector>
#include <optional>
#include <concepts>
#include <ranges>
#endif
#include <cassert>

export module Gromada.SoftwareRenderer;

import std;
import utils;
import Gromada.Resources;


//using Rgb565 = std::uint16_t;
export using FramebufferRef = std::mdspan<RGBA8, std::dextents<int, 2>>;
export void DrawSprite(const VidGraphics& data, std::size_t spriteIndex, int x, int y, FramebufferRef framebuffer);


// Implementation

// TODO: use full-featured 2D AABB's instead
struct BoundingBox {
	int left;
	int right;
	int top;
	int down;
};


void DrawSprite_Type0(const VidGraphics& data, BoundingBox srcBounds, std::size_t spriteIndex, int x, int y, FramebufferRef framebuffer) {
	std::mdspan indexedImage{
		reinterpret_cast<const std::uint8_t*>(data.frames[spriteIndex].data.data()),
		data.imgHeight,
		data.imgWidth,
	};
	// TODO: check is it produces decent asm
	for (int j = srcBounds.top; j < srcBounds.down; ++j) {
		for (int i = srcBounds.left; i < srcBounds.right; ++i) {
			framebuffer[y + j, x + i] = data.getPaletteColor(indexedImage[j, i]);
		}
	}
}

void DrawSprite_Type2(const VidGraphics& data, BoundingBox srcBounds, std::size_t spriteIndex, int x0, int y0, FramebufferRef framebuffer) {
	SpanStreamReader reader{data.frames[spriteIndex].data};

	const auto startY = reader.read<std::uint16_t>();
	const auto height = reader.read<std::uint16_t>();

	// Temporary
	if (srcBounds.right - srcBounds.left < data.imgWidth || srcBounds.down - srcBounds.top < data.imgHeight) {
		return;
	}

	for (std::size_t y = startY + y0; y < startY + height + y0; ++y) {
		std::size_t x = x0;
		while (true) {
			const auto commandByte = reader.read<std::uint8_t>();
			if (commandByte == 0)
				break;

			const auto count = commandByte & 0x3F;
			switch (commandByte & 0xC0) {
			case 0x00: {
				x += count;
			} break;
			case 0x40: {
				for (auto i = 0; i < count; ++i) {
					constexpr std::uint8_t ShadowMask = 0b00011111;
					auto oldColor = framebuffer[y, x];
					framebuffer[y, x++] = {static_cast<std::uint8_t>(oldColor.r & ShadowMask), static_cast<std::uint8_t>(oldColor.g & ShadowMask),
						static_cast<std::uint8_t>(oldColor.b & ShadowMask), 255};
				}
			} break;
			case 0x80: {
				for (auto i = 0; i < count; ++i) {
					const auto index = reader.read<std::uint8_t>();
					framebuffer[y, x++] = data.getPaletteColor(index);
				}
			} break;
			case 0xC0: {
				const auto index = reader.read<std::uint8_t>();
				for (auto i = 0; i < count; ++i) {
					framebuffer[y, x++] = data.getPaletteColor(index);
				}
			} break;
			default:
				assert(false && "Invalid command byte");
				break;
			}

		}
	}
}

// TODO: source data must be somehow validated before using (after loading?), because it may cause out of range access
void DrawSprite(const VidGraphics& data, std::size_t spriteIndex, int x, int y, FramebufferRef framebuffer) {
	assert(x > std::numeric_limits<int>::min()/2 && y > std::numeric_limits<int>::min()/2);
	assert(x < std::numeric_limits<int>::max() / 2 && y < std::numeric_limits<int>::max() / 2);

	if (data.frames.size() <= spriteIndex) {
		throw std::out_of_range("Sprite index out of range");
	}

	if (x + data.imgWidth < 0 || y + data.imgHeight < 0 || x > framebuffer.extent(1) || y > framebuffer.extent(0)) {
		return;
	}

	if (data.frames[spriteIndex].referenceFrameNumber != 0xFFFF) {
		spriteIndex = data.frames[spriteIndex].referenceFrameNumber;
	}

	const BoundingBox srcBounds{
		.left = std::max(-x, 0),
		.right = static_cast<int>(data.imgWidth) - std::max(x + static_cast<int>(data.imgWidth) - framebuffer.extent(1), 0),
		.top = std::max(-y, 0),
		.down = static_cast<int>(data.imgHeight) - std::max(y + static_cast<int>(data.imgHeight) - framebuffer.extent(0), 0),
	};

	SpanStreamReader reader{data.frames[spriteIndex].data};
	switch (data.visualBehavior) {
	case 0:
		DrawSprite_Type0(data, srcBounds, spriteIndex, x, y, framebuffer);
		break;
	case 2:
		DrawSprite_Type2(data, srcBounds, spriteIndex, x, y, framebuffer);
		break;
	default:
		for (int j = srcBounds.top; j < srcBounds.down; ++j) {
			for (int i = srcBounds.left; i < srcBounds.right; ++i) {
				framebuffer[y + j, x + i] = {255, 0, 0, 64};
			}
		}

		break;
	}
}

