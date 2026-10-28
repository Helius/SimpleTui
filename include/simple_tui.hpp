#pragma once

// simple_tui.hpp — header-only library intended for Arduino use.
//
// Keep everything in here free of desktop-only / Arduino-incompatible
// dependencies (no <iostream>, no STL that AVR can't handle, etc.)
// so the same header compiles on both desktop tests and the MCU.

#include <stdint.h>

namespace simple_tui {

// Output backend abstraction. Implementations (LCD, OLED, terminal, etc.)
// provide this so SimpleTui can emit text without knowing the hardware.
class Display {
public:
    virtual ~Display() = default;
    virtual void clearScreen() const = 0;
    virtual void print(uint16_t x, uint16_t y, const char* str) const = 0;
};

// Terminal dimensions holder as a class template. Width and height are
// compile-time constants — no runtime storage, no setters, AVR-friendly.
// A const reference to a Display backend is injected via the constructor.
template <uint16_t Width, uint16_t Height>
class SimpleTui {
public:
    explicit SimpleTui(const Display& display)
        : display_(display) {
        display_.clearScreen();
    }

    static constexpr uint16_t width()  { return Width; }
    static constexpr uint16_t height() { return Height; }

    const Display& display() const { return display_; }

private:
    const Display& display_;
};

} // namespace simple_tui
