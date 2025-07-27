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
import engine.bounding_box;
import Gromada.Resources;


export struct RGBA8 : ColorRgb8 {
	std::uint8_t a = 255;
};
export using FramebufferRef = std::mdspan<RGBA8, std::dextents<int, 2>>;
export void DrawSprite(const VidGraphics::Frame& frame, int x, int y, FramebufferRef framebuffer);


// Implementation

constexpr std::uint8_t lerp(std::uint8_t a, std::uint8_t b, std::uint8_t t) noexcept {
	const int mult = 0x10000 * t / 255;
	const int rem = 0x10000 - mult;
	return static_cast<std::uint8_t>((a * rem + b * mult) / 0x10000);
};

template <typename T>
concept CanvasInterfaceConcept = requires(T canvas, int sourceX, int sourceY, int count, std::span<const std::byte> colors_data, std::uint8_t color, std::uint8_t alpha) { //TODO: span<const uint8_t>
    { canvas.set_position(sourceX, sourceY) };
    { canvas.draw_pixels_shadow(sourceX, count) };
    { canvas.draw_pixels_indexed(sourceX, colors_data) };
	{ canvas.draw_pixels_repeat(sourceX, count, color) };
	{ canvas.draw_pixels_repeat_mode4(sourceX, count, std::uint8_t{}, std::uint8_t{}) };
	{ canvas.draw_pixels_alpha_blend(sourceX, alpha, colors_data) };
};

void Decode_mode0(VidGraphics::Frame frame, int maxHeight, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    for (int j = 0; j < std::min<int>(frame.height(), maxHeight); ++j) { // TODO : clip by y somehow at the decode stage to avoid unnecessary reads? [0,maxHeight] is too big interval
        canvas.set_position(0, j);
        canvas.draw_pixels_indexed(0, reader.read_bytes(frame.width()));
    }
}

void Decode_mode2(VidGraphics::Frame frame, int maxHeight, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, maxHeight);

    struct ControlWord {
        std::uint8_t count : 6;
        std::uint8_t command : 2;
    };

    for (int y = startY; y < endY; ++y) {
        canvas.set_position(0, y);
        auto sourceX = 0;
        for (ControlWord commandWord; commandWord = reader.read<ControlWord>(), commandWord.count != 0;) {
            switch (commandWord.command) {
            case 0:
                break;  // Do nothing, just skip pixels
            case 1:
                canvas.draw_pixels_shadow( sourceX, commandWord.count); break;
            case 2:
                canvas.draw_pixels_indexed( sourceX, reader.read_bytes(commandWord.count)); break;
            case 3:
                canvas.draw_pixels_repeat( sourceX, commandWord.count, reader.read<std::uint8_t>()); break;
            default:
                std::unreachable();
            }

            sourceX += commandWord.count;
        }

        assert(sourceX <= frame.width()); // Should not exceed the width of the sprite
    }
}

void Decode_mode4(VidGraphics::Frame frame, int maxHeight, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, maxHeight);

    struct ControlWord {
        std::uint16_t count : 7;
        std::uint16_t command : 3;
        std::uint16_t factor : 6;
    };

    for (int y = startY; y < endY; ++y) {
        canvas.set_position(0, y);
        auto sourceX = 0;
        for (ControlWord commandWord; commandWord = reader.read<ControlWord>(), commandWord.count > 0;) {
            if (commandWord.factor != 0 || commandWord.command != 0) {
                canvas.draw_pixels_repeat_mode4( sourceX, commandWord.count, commandWord.factor, commandWord.command);
            }

            sourceX += commandWord.count;
        }
    }
}

void Decode_Type8(VidGraphics::Frame frame, int maxHeight, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, maxHeight);

    struct ControlWord {
        std::uint8_t count : 5;
        std::uint8_t opacity : 3;
    };

    for (int y = startY; y < endY; ++y) {
        canvas.set_position(0, y);
        auto sourceX = 0;
        for (ControlWord command; command = reader.read<ControlWord>(), command.count != 0;) {
            if (command.opacity == 7) {
                canvas.draw_pixels_indexed(sourceX, reader.read_bytes(command.count));
            } else if (command.opacity != 0) {
                const std::uint8_t t = 255 - command.opacity * 42;
                canvas.draw_pixels_alpha_blend(sourceX, t, reader.read_bytes(command.count));
            }

            sourceX += command.count;
        }
    }
}

struct Canvas {
    int x0, y0;
    std::span<const ColorRgb8, 256> palette;
    FramebufferRef framebuffer;
    int y = 0;

    void set_position(int _, int sourceY) noexcept {
        y = y0 + sourceY;
    }

    void draw_pixels_shadow(int sourceX, int count) noexcept {
        for_clipped_pixels(sourceX, count, [&](int x, int i) {
            constexpr std::uint8_t ShadowMask = 0b11000011;
            auto oldColor = framebuffer[y, x];
            framebuffer[y, x] = {static_cast<std::uint8_t>(oldColor.r & ShadowMask), static_cast<std::uint8_t>(oldColor.g & ShadowMask),
                static_cast<std::uint8_t>(oldColor.b & ShadowMask), 255};
        });
    }

    void draw_pixels_indexed(int sourceX, std::span<const std::byte> colors_data) noexcept {
        for_clipped_pixels(sourceX, colors_data.size(), [&](int x, int i) {
            const auto color_index = static_cast<std::uint8_t>(colors_data[i]);
            framebuffer[y, x] = RGBA8{palette[color_index], 255};
        });
    }

    void draw_pixels_repeat(int sourceX, int count, std::uint8_t color_index) noexcept {
        for_clipped_pixels(sourceX, count, [&](int x, int i) {
            framebuffer[y, x] = RGBA8{palette[color_index], 255};
        });
    }

    //TODO: make beautiful effect instead just filling with color
    void draw_pixels_repeat_mode4(int sourceX, int count, std::uint8_t p1, std::uint8_t p2) noexcept {
        for_clipped_pixels(sourceX, count, [&](int x, int i) {
            framebuffer[y, x] = {static_cast<std::uint8_t>(p1 * 4), 0, static_cast<std::uint8_t>(1 << p2), 255};
        });
    }

    void draw_pixels_alpha_blend(int sourceX, std::uint8_t t, std::span<const std::byte> colors_data) noexcept {
        for_clipped_pixels(sourceX, colors_data.size(), [&](int x, int i) {
            const auto color_index = static_cast<std::uint8_t>(colors_data[i]);
            const auto srcColor = RGBA8{palette[color_index], 255};
            const auto dstColor = framebuffer[y, x];
            framebuffer[y, x] = {lerp(srcColor.r, dstColor.r, t), lerp(srcColor.g, dstColor.g, t), lerp(srcColor.b, dstColor.b, t), 255};
        });
    }

private:
	void for_clipped_pixels(int sourceX, int count, auto callback) noexcept {
        if (y < 0 || y >= framebuffer.extent(0)) {
            return; // Out of bounds
        }

        const int x = x0 + sourceX;
		for (int destination_x = std::max(x, 0); destination_x < std::min(x + count, framebuffer.extent(1)); ++destination_x) {
			callback(destination_x, destination_x - x);
        }
    }
};

void DrawSprite(const VidGraphics::Frame& frame, int x, int y, FramebufferRef framebuffer) {
    assert(x > std::numeric_limits<int>::min() / 2 && y > std::numeric_limits<int>::min() / 2);
	assert(x < std::numeric_limits<int>::max() / 2 && y < std::numeric_limits<int>::max() / 2);

	const VidGraphics& data = *frame.parent;
	if (x + data.width < 0 || y + data.height < 0 || x > framebuffer.extent(1) || y > framebuffer.extent(0)) {
		return;
	}

	Canvas canvas{x, y, std::span{data.palette}, framebuffer};

    switch (data.dataFormat) {
    case 0:
        return Decode_mode0(frame, framebuffer.extent(0), canvas);
    case 2:
        return Decode_mode2(frame, framebuffer.extent(0), canvas);
    case 4:
        return Decode_mode4(frame, framebuffer.extent(0), canvas);
    case 8:
        return Decode_Type8(frame, framebuffer.extent(0), canvas);
    };
}