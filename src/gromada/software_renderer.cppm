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
import engine.bounding_box; // TODO: bounding_box move to Gromada or ether utils
import Gromada.Resources;
import Gromada.GraphicsDecoder;

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


struct FramebufferCanvas {
    int x0, y0;
    std::span<const ColorRgb8, 256> palette;
    FramebufferRef framebuffer;
    int y = 0;

    [[nodiscard]] ClippingInfo get_clipping(BoundingBox source_rect) const noexcept {
        const BoundingBox destination_rect {-x0, framebuffer.extent(1)-x0, -y0, framebuffer.extent(0)-y0};
        return {source_rect, destination_rect};
    }

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
    if (x + data.width <= 0 || y + data.height <= 0 || x >= framebuffer.extent(1) || y >= framebuffer.extent(0)) {
        return;
    }

    FramebufferCanvas canvas{x, y, std::span{data.palette}, framebuffer};
    DecodeFrame(frame, canvas);
}