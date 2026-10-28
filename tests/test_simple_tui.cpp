#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <simple_tui.hpp>
#include <stdint.h>

#include <string>
#include <vector>

class TestDisplay : public simple_tui::Display {
public:
    void print(uint16_t x, uint16_t y, const char* str) const override {
        if (buffer.size() <= y)
            buffer.resize(y + 1);
        auto& row = buffer[y];
        if (row.size() <= x)
            row.resize(x + 1, " ");
        row[x] = (str != nullptr) ? str : "";
    }

    std::vector<std::string> lines() const {
        std::vector<std::string> out;
        for (const auto& row : buffer) {
            std::string s;
            for (const auto& cell : row)
                s += cell;
            out.push_back(s);
        }
        return out;
    }

    mutable std::vector<std::vector<std::string>> buffer;
};

TEST_CASE("SimpleTui constructor draws the border") {
    TestDisplay display;
    simple_tui::SimpleTui<5, 4> tui(display);

    const std::vector<std::string> expected = {
        "┌───┐",
        "│   │",
        "│   │",
        "└───┘",
    };

    CHECK(display.lines() == expected);
}

TEST_CASE("SimpleTui::drawRect draws a rectangle at an offset") {
    TestDisplay display;
    simple_tui::SimpleTui<7, 5> tui(display);

    tui.drawRect(2, 1, 3, 3);

    const std::vector<std::string> expected = {
        "┌─────┐",
        "│ ┌─┐ │",
        "│ │ │ │",
        "│ └─┘ │",
        "└─────┘",
    };

    CHECK(display.lines() == expected);
}

TEST_CASE("SimpleTui::print renders into the grid") {
    TestDisplay display;
    simple_tui::SimpleTui<5, 4> tui(display);

    tui.print(2, 1, "X");

    const std::vector<std::string> expected = {
        "┌───┐",
        "│ X │",
        "│   │",
        "└───┘",
    };

    CHECK(display.lines() == expected);
}

TEST_CASE("SimpleTui skips drawing for degenerate sizes") {
    TestDisplay display;
    simple_tui::SimpleTui<1, 1> tui(display);
    (void) tui;

    CHECK(display.lines().empty());
}