module;
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

constexpr std::uint8_t multiply(std::uint8_t a, std::uint8_t factor) {
    assert(factor >= 0 && factor < 8);
    [[assume(factor >= 0 && factor < 8)]];
    return std::clamp((a * 8) / (8-factor), 0, 255);
}


struct SoftwareRendererVisitor {
    int x0, y0;
    std::span<const ColorRgb8, 256> palette;
    FramebufferRef framebuffer;
    BoundingBox source_rect; // just for validation purposes
    int x = 0, y = 0;

    [[nodiscard]] ClippingInfo begin_image(BoundingBox source_rect) noexcept {
        this->source_rect = source_rect;
        const BoundingBox destination_rect {-x0, framebuffer.extent(1)-x0, -y0, framebuffer.extent(0)-y0};
        return {source_rect, destination_rect};
    }

    void set_cursor(int cursor_x, int cursor_y) noexcept {
        x = x0 + cursor_x;
        y = y0 + cursor_y;
        assert(source_rect.isPointInside(cursor_x, cursor_y));
    }

    void advance_cursor(int count) noexcept {
        x += count;
        assert(source_rect.isPointInside(x - x0, y - y0));
    }

    void draw_pixels_shadow(int count) noexcept {
        for_clipped_pixels( count, [&](int x, int i) {
            constexpr std::uint8_t ShadowMask = 0b11000011;
            auto oldColor = framebuffer[y, x];
            framebuffer[y, x] = {static_cast<std::uint8_t>(oldColor.r & ShadowMask), static_cast<std::uint8_t>(oldColor.g & ShadowMask),
                static_cast<std::uint8_t>(oldColor.b & ShadowMask), 255};
        });
    }

    void draw_pixels_indexed(std::span<const IndexedColor> colors_data) noexcept {
        for_clipped_pixels(colors_data.size(), [&](int x, int i) {
            const auto color_index = std::to_underlying(colors_data[i]);
            framebuffer[y, x] = RGBA8{palette[color_index], 255};
        });
    }

    void draw_pixels(std::span<const CompressedColor> colors_data) noexcept {
        for_clipped_pixels(colors_data.size(), [&](int x, int i) {
            framebuffer[y, x] = RGBA8{colors_data[i].to_rgb8(), 255};
        });
    }

    void draw_pixels_repeat(int count, IndexedColor color_index) noexcept {
        for_clipped_pixels( count, [this, color = RGBA8{palette[std::to_underlying(color_index)], 255}](int x, int i) {
            framebuffer[y, x] = color;
        });
    }

    void draw_pixels_repeat(int count, CompressedColor color) noexcept {
        for_clipped_pixels( count, [this, color = RGBA8{color.to_rgb8(), 255}](int x, int i) {
            framebuffer[y, x] = color;
        });
    }

    void draw_pixels_light(int count, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        for_clipped_pixels( count, [&](int x, int i) {
            const auto src = framebuffer[y, x];

            framebuffer[y, x] = {multiply(src.r, r), multiply(src.g, g), multiply(src.b, b) , 255};
        });
    }

    void draw_pixels_alpha_blend(std::uint8_t t, std::span<const std::byte> colors_data) noexcept {
        for_clipped_pixels(colors_data.size(), [&](int x, int i) {
            const auto color_index = static_cast<std::uint8_t>(colors_data[i]);
            const auto srcColor = RGBA8{palette[color_index], 255};
            const auto dstColor = framebuffer[y, x];
            framebuffer[y, x] = {lerp(srcColor.r, dstColor.r, t), lerp(srcColor.g, dstColor.g, t), lerp(srcColor.b, dstColor.b, t), 255};
        });
    }

private:
    void for_clipped_pixels( int count, auto callback) noexcept {
        if (y < 0 || y >= framebuffer.extent(0)) {
            return; // Out of bounds
        }

        for (int destination_x = std::max(x, 0); destination_x < std::min(x + count, framebuffer.extent(1)); ++destination_x) {
            callback(destination_x, destination_x - x);
        }

        advance_cursor(count);
    }
};

void DrawSprite(const VidGraphics::Frame& frame, int x, int y, FramebufferRef framebuffer) {
    assert(x > std::numeric_limits<int>::min() / 2 && y > std::numeric_limits<int>::min() / 2);
    assert(x < std::numeric_limits<int>::max() / 2 && y < std::numeric_limits<int>::max() / 2);

    const VidGraphics& data = *frame.parent;
    if (x + data.width <= 0 || y + data.height <= 0 || x >= framebuffer.extent(1) || y >= framebuffer.extent(0)) {
        return;
    }

    SoftwareRendererVisitor renderer{x, y, std::span{data.palette}, framebuffer};
    DecodeFrame(frame, renderer);
}