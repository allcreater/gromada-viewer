module;
#include <cassert>

export module Gromada.GraphicsDecoder;

import std;
import Gromada.Resources;
import engine.bounding_box;

export {
    using IndexedColor = std::byte;
    struct CompressedColor {
		std::uint16_t b : 5;
		std::uint16_t g : 5;
		std::uint16_t r : 5;
		std::uint16_t a : 1;

		[[nodiscard]] constexpr ColorRgb8 to_rgb8() const noexcept {
            return {
                static_cast<std::uint8_t>(r * 255 / 31),
                static_cast<std::uint8_t>(g * 255 / 31),
                static_cast<std::uint8_t>(b * 255 / 31)
            };
        }
    };

    struct ClippingInfo {
        int skip_first_x, skip_last_x, skip_first_y, skip_last_y;
        int last_x, last_y;

        ClippingInfo (BoundingBox source_rect, BoundingBox destination_rect) noexcept;
    };

    template <typename T>
    concept DecoderVisitor = requires(T visitor, int sourceX, int sourceY, int count, std::span<const IndexedColor> color_indices, std::span<const CompressedColor> colors, std::uint8_t alpha, BoundingBox source_rect) {
        { visitor.begin_image(source_rect) } -> std::same_as<ClippingInfo>;
        { visitor.set_cursor(sourceX, sourceY) };
        { visitor.advance_cursor(count) };
        { visitor.draw_pixels_shadow(count) };
        { visitor.draw_pixels(colors) };
        { visitor.draw_pixels_indexed(color_indices) };
        { visitor.draw_pixels_repeat(count, IndexedColor{}) };
        { visitor.draw_pixels_repeat(count, CompressedColor{}) };
        { visitor.draw_pixels_light(count, std::uint8_t{}, std::uint8_t{}, std::uint8_t{}) };
        { visitor.draw_pixels_alpha_blend(alpha, color_indices) };
    };

    void DecodeFrame(const VidGraphics::Frame& frame, DecoderVisitor auto visitor);
}


// Implementation

void Decode_mode0(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    const auto stride = frame.width();

    for (int y = clipping_info.skip_first_y; y != clipping_info.last_y; ++y) {
        visitor.set_cursor(0, y);
        visitor.draw_pixels_indexed(frame.data.subspan(y * stride, stride));
    }
}

void Decode_mode2(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

    struct ControlWord {
        std::uint8_t count : 6;
        std::uint8_t command : 2;
    };

    for (int y = startY; y < endY; ++y) {
        visitor.set_cursor(0, y);
        for (ControlWord command; command = reader.read<ControlWord>(), command.count != 0;) {
            switch (command.command) {
            case 0:
                visitor.advance_cursor(command.count); break;
            case 1:
                visitor.draw_pixels_shadow( command.count); break;
            case 2:
                visitor.draw_pixels_indexed( reader.read_bytes(command.count)); break;
            case 3:
                visitor.draw_pixels_repeat( command.count, reader.read<IndexedColor>()); break;
            default:
                std::unreachable();
            }
        }
    }
}

void Decode_mode3(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

    struct ControlWord {
		std::uint8_t count : 5;
		std::uint8_t factor : 3;
	};

    for (int y = startY; y < endY; ++y) {
        visitor.set_cursor(0, y);
        for (ControlWord command; command = reader.read<ControlWord>(), command.count != 0;) {
            if (command.factor) {
                visitor.draw_pixels_light(command.count, command.factor, command.factor, command.factor);
            } else {
                visitor.advance_cursor(command.count);
            }
        }
    }
}

void Decode_mode4(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

    struct ControlWord {
        std::uint16_t count : 7;
		std::uint16_t b_factor : 3;
		std::uint16_t g_factor : 3;
		std::uint16_t r_factor : 3;
	};

    for (int y = startY; y < endY; ++y) {
        visitor.set_cursor(0, y);
        for (ControlWord command; command = reader.read<ControlWord>(), command.count > 0;) {
            if (command.r_factor != 0 || command.g_factor != 0 || command.b_factor != 0) {
                visitor.draw_pixels_light( command.count, command.r_factor, command.g_factor, command.b_factor);
            } else {
                visitor.advance_cursor(command.count);
            }
        }
    }
}

void Decode_mode6(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    const auto stride = frame.width();
    auto reader = frame.read();

    struct ControlWord {
        std::uint8_t count : 7;
        std::uint8_t repeat : 1;
    };

    for (int y = 0; y < frame.height(); ++y) {
        visitor.set_cursor(0, y);
        for (int x = 0; x < stride; ) {
            ControlWord command = reader.read<ControlWord>();
            if (command.repeat) {
                visitor.draw_pixels_repeat( command.count, reader.read<CompressedColor>());
            } else {
                // TODO: use std::start_lifetime_as_array when it will be available
                const auto bytes = reader.read_bytes(2 * command.count);
                std::array<CompressedColor, 128> colors; assert(command.count < colors.size());
                std::memcpy(colors.data(), bytes.data(), bytes.size_bytes());

                visitor.draw_pixels(std::span{colors.data(), command.count});
            }
            x+= command.count;
        }
    }
}

void Decode_Type8(VidGraphics::Frame frame, ClippingInfo clipping_info, DecoderVisitor auto visitor) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

    struct ControlWord {
        std::uint8_t count : 5;
        std::uint8_t opacity : 3;
    };

    for (int y = startY; y < endY; ++y) {
        visitor.set_cursor(0, y);
        for (ControlWord command; command = reader.read<ControlWord>(), command.count != 0;) {
            if (command.opacity == 7) {
                visitor.draw_pixels_indexed(reader.read_bytes(command.count));
            } else if (command.opacity != 0) {
                const std::uint8_t t = 255 - command.opacity * 42;
                visitor.draw_pixels_alpha_blend( t, reader.read_bytes(command.count));
            } else {
                visitor.advance_cursor(command.count);
            }
        }
    }
}

ClippingInfo::ClippingInfo (BoundingBox source_rect, BoundingBox destination_rect) noexcept
    : skip_first_x{ std::max(0, destination_rect.left - source_rect.left)}
    , skip_last_x {std::max( 0, source_rect.right - destination_rect.right)}
    , skip_first_y{ std::max(0, destination_rect.top - source_rect.top)}
    , skip_last_y {std::max( 0, source_rect.down - destination_rect.down)}
    , last_x{ source_rect.right - skip_last_x }
    , last_y{ source_rect.down - skip_last_y }
{}

void DecodeFrame(const VidGraphics::Frame& frame, DecoderVisitor auto visitor) {
    // NOTE: Unfortunately data of formats 2, 4, 8 is compressed, so we can do clipping only by last_y
    const auto clipping_info = visitor.begin_image({0, frame.width(), 0, frame.height()});
    assert(clipping_info.skip_last_x >= 0 && clipping_info.skip_first_x >= 0 && clipping_info.skip_last_y >= 0 && clipping_info.skip_first_y >= 0);
    assert(clipping_info.skip_first_x + clipping_info.skip_last_x < frame.width());
    assert(clipping_info.skip_first_y + clipping_info.skip_last_y < frame.height());


    switch (frame.parent->dataFormat) {
    case 0:
        return Decode_mode0(frame, clipping_info, visitor);
    case 2:
        return Decode_mode2(frame, clipping_info, visitor);
    case 3:
		return Decode_mode3(frame, clipping_info, visitor);
	case 6:
        return Decode_mode6(frame, clipping_info, visitor);
	case 7: // Not supported yet
		return;
	case 4:
		return Decode_mode4(frame, clipping_info, visitor);
	case 8:
        return Decode_Type8(frame, clipping_info, visitor);
    default:
        throw std::runtime_error("Unknown graphics format");
    };
}