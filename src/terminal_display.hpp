#pragma once

// terminal_display.hpp — desktop-only Display backend for src/main.cpp.
//
// Writes to stdout using ANSI escape sequences: move the cursor to the
// requested cell, then print only that string. This avoids repainting the
// whole screen on every cell update.
//
// Not Arduino-safe on purpose (uses <iostream>); it lives in src/, not in
// the header-only library.

#include <simple_tui.hpp>

#include <cstdint>
#include <iostream>

class StdoutDisplay : public stui::Display {
public:
    void clear() const override {
        // Reset colors, erase the whole screen, then move the cursor home.
        std::cout << "\x1b[0m\x1b[2J\x1b[H" << std::flush;
    }

    void setColor(stui::Color color) const override {
        std::cout << "\x1b[38;2;"
                  << static_cast<unsigned>(color.r) << ';'
                  << static_cast<unsigned>(color.g) << ';'
                  << static_cast<unsigned>(color.b) << 'm'
                  << std::flush;
    }

    void setColors(const stui::Colors& colors) const override {
        std::cout << "\x1b[38;2;"
                  << static_cast<unsigned>(colors.fg.r) << ';'
                  << static_cast<unsigned>(colors.fg.g) << ';'
                  << static_cast<unsigned>(colors.fg.b) << 'm'
                  << "\x1b[48;2;"
                  << static_cast<unsigned>(colors.bg.r) << ';'
                  << static_cast<unsigned>(colors.bg.g) << ';'
                  << static_cast<unsigned>(colors.bg.b) << 'm'
                  << std::flush;
    }

    void print(uint16_t x, uint16_t y, const char* str) const override {
        // ANSI cursor positions are 1-based: ESC [ row ; column H
        std::cout << "\x1b[" << (static_cast<unsigned>(y) + 1)
                  << ';' << (static_cast<unsigned>(x) + 1)
                  << 'H' << (str != nullptr ? str : "")
                  << std::flush;
    }
};