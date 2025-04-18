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
//export struct RGBA8;
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

constexpr std::uint8_t lerp(std::uint8_t a, std::uint8_t b, std::uint8_t t) {
	const int mult = 0x10000 * t / 255;
	const int rem = 0x10000 - mult;
	return static_cast<std::uint8_t>((a * rem + b * mult) / 0x10000);
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

	const int startY = static_cast<int>(reader.read<std::uint16_t>());
	const int height = reader.read<std::uint16_t>();

	// Temporary
	if (srcBounds.right - srcBounds.left < data.imgWidth || startY - srcBounds.top < 0) {
		return;
	}

	for (int y = startY + y0, endY = std::min(startY + height + y0, framebuffer.extent(0)); y < endY; ++y) {
		int x = x0;
		for (std::uint8_t commandByte; commandByte = reader.read<std::uint8_t>(), commandByte != 0;) {

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

void DrawSprite_Type4(const VidGraphics& data, BoundingBox srcBounds, std::size_t spriteIndex, int x0, int y0, FramebufferRef framebuffer) {
	SpanStreamReader reader{data.frames[spriteIndex].data};

	const int startY = static_cast<int>(reader.read<std::uint16_t>());
	const int height = reader.read<std::uint16_t>();

	// Temporary
	if (srcBounds.right - srcBounds.left < data.imgWidth || startY - srcBounds.top < 0) {
		return;
	}

	struct ControlWord {
		std::uint16_t count : 7;
		std::uint16_t command : 3;
		std::uint16_t factor : 6;
	};

	for (int y = startY + y0, endY = std::min(startY + height + y0, framebuffer.extent(0)); y < endY; ++y) {
		int x = x0;
		for (ControlWord commandWord; commandWord = reader.read<ControlWord>(), commandWord.count > 0;) {
			if (commandWord.factor == 0 && commandWord.command == 0) {
				x += commandWord.count;
			}
			else {
				for (int i = 0; i < commandWord.count; ++i) {
					framebuffer[y, x++] = {static_cast<std::uint8_t>(commandWord.factor * 4), 0, static_cast<std::uint8_t>(1 << commandWord.command), 255};
				}
			}
		}
	}
}

void DrawSprite_Type8(const VidGraphics& data, BoundingBox srcBounds, std::size_t spriteIndex, int x0, int y0, FramebufferRef framebuffer) {	

	SpanStreamReader reader{data.frames[spriteIndex].data};

	const int startY = static_cast<int>(reader.read<std::uint16_t>());
	const int height = reader.read<std::uint16_t>();

	// Temporary
	if (srcBounds.right - srcBounds.left < data.imgWidth || startY - srcBounds.top < 0) {
		return;
	}

	for (int y = startY + y0, endY = std::min(startY + height + y0, framebuffer.extent(0)); y < endY; ++y) {
		int x = x0;
		for (std::uint8_t commandByte; commandByte = reader.read<std::uint8_t>(), commandByte != 0;) {
			const auto count = commandByte & 0x1F;
			
			if ((commandByte & 0xE0) == 0) {
				x += count;
			} else if ((commandByte & 0xE0) == 0x20) {
				for (auto i = 0; i < count; ++i) {
					const auto index = reader.read<std::uint8_t>();
					framebuffer[y, x++] = data.getPaletteColor(index);
				}
			} else {
				const std::uint8_t opacity = 255 - (commandByte & 0xE0);
				for (auto i = 0; i < count; ++i) {
					const auto index = reader.read<std::uint8_t>();
					const auto srcColor = data.getPaletteColor(index);
					const auto dstColor = framebuffer[y, x];
					framebuffer[y, x++] = {
						lerp(srcColor.r, dstColor.r, opacity), lerp(srcColor.g, dstColor.g, opacity), lerp(srcColor.b, dstColor.b, opacity), 255}; // 
				}
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
	case 4:
		DrawSprite_Type4(data, srcBounds, spriteIndex, x, y, framebuffer);
		break;
	case 8:
		DrawSprite_Type8(data, srcBounds, spriteIndex, x, y, framebuffer);
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

