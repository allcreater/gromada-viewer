module;
#include <cassert>

export module Gromada.GraphicsDecoder;

import std;
import Gromada.Resources;

export {
    template <typename T>
    concept CanvasInterfaceConcept = requires(T canvas, int sourceX, int sourceY, int count, std::span<const std::byte> colors_data, std::uint8_t color, std::uint8_t alpha) { //TODO: span<const uint8_t>
        { canvas.set_position(sourceX, sourceY) };
        { canvas.draw_pixels_shadow(sourceX, count) };
        { canvas.draw_pixels_indexed(sourceX, colors_data) };
        { canvas.draw_pixels_repeat(sourceX, count, color) };
        { canvas.draw_pixels_repeat_mode4(sourceX, count, std::uint8_t{}, std::uint8_t{}) };
        { canvas.draw_pixels_alpha_blend(sourceX, alpha, colors_data) };
    };

    void DecodeFrame(const VidGraphics::Frame& frame, CanvasInterfaceConcept auto canvas);
}


// Implementation

void Decode_mode0(VidGraphics::Frame frame, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    for (int j = 0; j < frame.height(); ++j) { // TODO : clip by y somehow at the decode stage to avoid unnecessary reads?
        canvas.set_position(0, j);
        canvas.draw_pixels_indexed(0, reader.read_bytes(frame.width()));
    }
}

void Decode_mode2(VidGraphics::Frame frame, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = reader.read<std::uint16_t>() + startY;

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

void Decode_mode4(VidGraphics::Frame frame, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = reader.read<std::uint16_t>() + startY;

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

void Decode_Type8(VidGraphics::Frame frame, CanvasInterfaceConcept auto canvas) {
    auto reader = frame.read();
    const int startY = static_cast<int>(reader.read<std::uint16_t>());
    const int endY = reader.read<std::uint16_t>() + startY;

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

void DecodeFrame(const VidGraphics::Frame& frame, CanvasInterfaceConcept auto canvas) {
    switch (frame.parent->dataFormat) {
    case 0:
        return Decode_mode0(frame, canvas);
    case 2:
        return Decode_mode2(frame, canvas);
    case 4:
        return Decode_mode4(frame, canvas);
    case 8:
        return Decode_Type8(frame, canvas);
    case 3:
    case 6:
    case 7:
        return; // Not supported yet
    default:
        throw std::runtime_error("Unknown graphics format");
    };
}