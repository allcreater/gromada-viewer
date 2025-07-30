module;
#include <cassert>

export module Gromada.GraphicsDecoder;

import std;
import Gromada.Resources;
import engine.bounding_box;

export {
    struct ClippingInfo {
        int skip_first_x, skip_last_x, skip_first_y, skip_last_y;
        int last_x, last_y;

        ClippingInfo (BoundingBox source_rect, BoundingBox destination_rect) noexcept;
    };

    template <typename T>
    concept CanvasInterfaceConcept = requires(T canvas, int sourceX, int sourceY, int count, std::span<const std::byte> colors_data, std::uint8_t color, std::uint8_t alpha, BoundingBox source_rect) { //TODO: span<const uint8_t>
        { canvas.get_clipping(source_rect) } -> std::same_as<ClippingInfo>;
        { canvas.set_position(sourceX, sourceY) };
        { canvas.draw_pixels_shadow(sourceX, count) };
        { canvas.draw_pixels_indexed(sourceX, colors_data) };
        { canvas.draw_pixels_repeat(sourceX, count, color) };
        { canvas.draw_pixels_light(sourceX, count, std::uint8_t{}, std::uint8_t{}, std::uint8_t{}) };
        { canvas.draw_pixels_alpha_blend(sourceX, alpha, colors_data) };
    };

    void DecodeFrame(const VidGraphics::Frame& frame, CanvasInterfaceConcept auto canvas);
}


// Implementation

void Decode_mode0(VidGraphics::Frame frame, ClippingInfo clipping_info, CanvasInterfaceConcept auto canvas) {
    const auto stride = frame.width();

    for (int y = clipping_info.skip_first_y; y != clipping_info.last_y; ++y) {
        canvas.set_position(0, y);
        canvas.draw_pixels_indexed(0, frame.data.subspan(y * stride, stride));
    }
}

void Decode_mode2(VidGraphics::Frame frame, ClippingInfo clipping_info, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

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

void Decode_mode3(VidGraphics::Frame frame, ClippingInfo clipping_info, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

    struct ControlWord {
		std::uint8_t count : 5;
		std::uint8_t factor : 3;
	};

    for (int y = startY; y < endY; ++y) {
        canvas.set_position(0, y);
        auto sourceX = 0;
        for (ControlWord commandWord; commandWord = reader.read<ControlWord>(), commandWord.count != 0;) {
            if (commandWord.factor) {
                canvas.draw_pixels_light(sourceX, commandWord.count, commandWord.factor, commandWord.factor, commandWord.factor);
            }

            sourceX += commandWord.count;
        }
    }
}

void Decode_mode4(VidGraphics::Frame frame, ClippingInfo clipping_info, CanvasInterfaceConcept auto canvas) {
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
        canvas.set_position(0, y);
        auto sourceX = 0;
        for (ControlWord commandWord; commandWord = reader.read<ControlWord>(), commandWord.count > 0;) {
            if (commandWord.r_factor != 0 || commandWord.g_factor != 0 || commandWord.b_factor != 0) {
                canvas.draw_pixels_light( sourceX, commandWord.count, commandWord.r_factor, commandWord.g_factor, commandWord.b_factor);
            }

            sourceX += commandWord.count;
        }
    }
}

void Decode_Type8(VidGraphics::Frame frame, ClippingInfo clipping_info, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = std::min(reader.read<std::uint16_t>() + startY, clipping_info.last_y);

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

ClippingInfo::ClippingInfo (BoundingBox source_rect, BoundingBox destination_rect) noexcept
    : skip_first_x{ std::max(0, destination_rect.left - source_rect.left)}
    , skip_last_x {std::max( 0, source_rect.right - destination_rect.right)}
    , skip_first_y{ std::max(0, destination_rect.top - source_rect.top)}
    , skip_last_y {std::max( 0, source_rect.down - destination_rect.down)}
    , last_x{ source_rect.right - skip_last_x }
    , last_y{ source_rect.down - skip_last_y }
{}

void DecodeFrame(const VidGraphics::Frame& frame, CanvasInterfaceConcept auto canvas) {
    // NOTE: Unfortunately data of formats 2, 4, 8 is compressed, so we can do clipping only by last_y
    const auto clipping_info = canvas.get_clipping({0, frame.width(), 0, frame.height()});
    assert(clipping_info.skip_last_x >= 0 && clipping_info.skip_first_x >= 0 && clipping_info.skip_last_y >= 0 && clipping_info.skip_first_y >= 0);
    assert(clipping_info.skip_first_x + clipping_info.skip_last_x < frame.width());
    assert(clipping_info.skip_first_y + clipping_info.skip_last_y < frame.height());


    switch (frame.parent->dataFormat) {
    case 0:
        return Decode_mode0(frame, clipping_info, canvas);
    case 2:
        return Decode_mode2(frame, clipping_info, canvas);
    case 3:
        return Decode_mode3(frame, clipping_info, canvas);
    case 4:
        return Decode_mode4(frame, clipping_info, canvas);
    case 8:
        return Decode_Type8(frame, clipping_info, canvas);
    case 6:
    case 7:
        return; // Not supported yet
    default:
        throw std::runtime_error("Unknown graphics format");
    };
}