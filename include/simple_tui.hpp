#pragma once

#include <stdint.h>

namespace simple_tui {

class Display {
public:
    virtual ~Display() = default;
    virtual void print(uint16_t x, uint16_t y, const char* str) const = 0;
};

template <uint16_t Width, uint16_t Height>
class SimpleTui {
public:
    explicit SimpleTui(const Display& display)
        : display_(display) {
        drawRect(0, 0, Width, Height);
    }

    static constexpr uint16_t width()  { return Width; }
    static constexpr uint16_t height() { return Height; }

    const Display& display() const { return display_; }

    void print(uint16_t x, uint16_t y, const char* str) const {
        display_.print(x, y, str);
    }

    void drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const {
        if (width < 2 || height < 2) return;

        display_.print(x, y, "┌");
        for (uint16_t i = 1; i < width - 1; ++i)
            display_.print(x + i, y, "─");
        display_.print(x + width - 1, y, "┐");

        for (uint16_t j = 1; j < height - 1; ++j) {
            display_.print(x, y + j, "│");
            display_.print(x + width - 1, y + j, "│");
        }

        display_.print(x, y + height - 1, "└");
        for (uint16_t i = 1; i < width - 1; ++i)
            display_.print(x + i, y + height - 1, "─");
        display_.print(x + width - 1, y + height - 1, "┘");
    }

private:
    const Display& display_;
};

} // namespace simple_tui