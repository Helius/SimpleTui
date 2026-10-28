#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <simple_tui.hpp>
#include <stdint.h>

#include <sstream>
#include <string>
#include <vector>

#include "terminal_display.hpp"

class TestDisplay : public stui::Display {
public:
    void clear() const override { clear_count++; }

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

    void reset() const { buffer.clear(); }

    mutable std::vector<std::vector<std::string>> buffer;
    mutable int clear_count = 0;
};

// ---------------------------------------------------------------------------
// StdoutDisplay — ANSI output backend
// ---------------------------------------------------------------------------

TEST_CASE("StdoutDisplay::clear wipes the terminal and homes the cursor") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.clear();

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[2J\x1b[H");
}

TEST_CASE("StdoutDisplay::print moves the cursor to x,y and prints only that string") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.print(3, 2, "hi");

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[3;4Hhi");
}

TEST_CASE("StdoutDisplay::print emits no text for a null string") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.print(0, 0, nullptr);

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[1;1H");
}

TEST_CASE("Text measure sets width to length and height to one") {
    stui::Text t{"hello"};
    t.measure(80, 24);
    CHECK(t.measured_width() == 5);
    CHECK(t.measured_height() == 1);
}

TEST_CASE("Text render prints its string at the assigned box") {
    stui::Text t{"hello"};
    t.measure(80, 24);
    t.layout(0, 0, 80, 1);

    TestDisplay display;
    t.render(display);

    const std::vector<std::string> expected = {"hello"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Text elides with an ellipsis when wider than its box") {
    stui::Text t{"hello"};
    t.measure(80, 24);
    t.layout(0, 0, 3, 1);

    TestDisplay display;
    t.render(display);

    const std::vector<std::string> expected = {"he\xE2\x80\xA6"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Text renders nothing when its box has zero width") {
    stui::Text t{"hello"};
    t.measure(80, 24);
    t.layout(0, 0, 0, 1);

    TestDisplay display;
    t.render(display);

    CHECK(display.lines().empty());
}

TEST_CASE("Text scroll offset skips leading characters") {
    stui::Text t{"hello"};
    t.measure(80, 24);
    t.layout(0, 0, 80, 1);
    t.setScroll(2);

    TestDisplay display;
    t.render(display);

    const std::vector<std::string> expected = {"llo"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Column measure is max width by sum height") {
    stui::Column<stui::Text, stui::Text> col{stui::Text{"hello"}, stui::Text{"world"}};
    col.measure(80, 24);
    CHECK(col.measured_width() == 5);
    CHECK(col.measured_height() == 2);
}

TEST_CASE("Column stacks children vertically") {
    stui::Column<stui::Text, stui::Text> col{stui::Text{"hello"}, stui::Text{"world"}};
    col.measure(80, 24);
    col.layout(0, 0, 80, 2);

    TestDisplay display;
    col.render(display);

    const std::vector<std::string> expected = {"hello", "world"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Column shrinks fill children so it never exceeds its assigned height") {
    stui::Column<stui::Panel<stui::Text>, stui::Panel<stui::Text>> col{
        stui::Panel<stui::Text>{stui::Text{"a"}},
        stui::Panel<stui::Text>{stui::Text{"b"}}};

    col.measure(5, 6);
    CHECK(col.measured_width() == 5);
    CHECK(col.measured_height() == 6); // clamped to the available height

    col.layout(0, 0, 5, 6);

    TestDisplay display;
    col.render(display);

    // 6 tall -> 3 per panel, stacked.
    const std::vector<std::string> expected = {
        "┌───┐",
        "│a  │",
        "└───┘",
        "┌───┐",
        "│b  │",
        "└───┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Row measure is sum width plus spacing by max height") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    CHECK(row.measured_width() == 5); // 2 + 1 (default spacing) + 2
    CHECK(row.measured_height() == 1);
}

TEST_CASE("Row places children horizontally with one space by default") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    row.layout(0, 0, 80, 1);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {"ab cd"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Row custom spacing separates children") {
    stui::Row<stui::Text, stui::Text> row{2, stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    CHECK(row.measured_width() == 6); // 2 + 2 (spacing) + 2
    row.layout(0, 0, 6, 1);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {"ab  cd"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Row shrinks fill children so it never exceeds its assigned width") {
    stui::Row<stui::Panel<stui::Text>, stui::Panel<stui::Text>> row{
        4,
        stui::Panel<stui::Text>{stui::Text{"a"}},
        stui::Panel<stui::Text>{stui::Text{"b"}}};

    row.measure(10, 3);
    CHECK(row.measured_width() == 10); // clamped to the available width

    row.layout(0, 0, 10, 3);

    TestDisplay display;
    row.render(display);

    // 10 wide, 4 spacing -> 3 per panel; second starts at x = 7.
    const std::vector<std::string> expected = {
        "┌─┐    ┌─┐",
        "│a│    │b│",
        "└─┘    └─┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Panel fills the available parent size") {
    stui::Panel<stui::Text> panel{stui::Text{"hi"}};
    panel.measure(5, 3);
    CHECK(panel.measured_width() == 5);
    CHECK(panel.measured_height() == 3);
}

TEST_CASE("Panel draws a border and renders its children inside") {
    stui::Panel<stui::Text> panel{stui::Text{"hi"}};
    panel.measure(5, 3);
    panel.layout(0, 0, 5, 3);

    TestDisplay display;
    panel.render(display);

    const std::vector<std::string> expected = {
        "┌───┐",
        "│hi │",
        "└───┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Text scroll loops past the end with a gap before the start") {
    stui::Text t{"long string with end"};
    t.measure(14, 1);
    t.layout(0, 0, 14, 1);
    t.setScroll(17);

    TestDisplay display;
    t.render(display);

    // "long string with end", window 14: tail "end", 3-space gap, then "long str".
    const std::vector<std::string> expected = {"end   long str"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Text tick advances scroll and loops through the gap") {
    stui::Text t{"abcdef"};
    t.measure(3, 1);
    t.layout(0, 0, 3, 1);

    CHECK(t.scroll() == 0);
    t.tick();
    CHECK(t.scroll() == 1);
    t.tick();
    CHECK(t.scroll() == 2);
    for (int i = 0; i < 3; ++i) t.tick();
    CHECK(t.scroll() == 5);
    // "abcdef" + a three-space gap is a 9-step loop.
    t.tick();
    CHECK(t.scroll() == 6);
    t.tick();
    CHECK(t.scroll() == 7);
    t.tick();
    CHECK(t.scroll() == 8);
    t.tick();
    CHECK(t.scroll() == 0);
}

TEST_CASE("Text tick stays at zero when the text fits its box") {
    stui::Text t{"hi"};
    t.measure(3, 1);
    t.layout(0, 0, 3, 1);

    t.tick();
    CHECK(t.scroll() == 0);
}

TEST_CASE("SimpleTui::loop redraws only after the 500 ms interval and animates") {
    TestDisplay display;
    stui::SimpleTui<3, 1> tui(display);
    stui::Text t{"abcdef"};

    tui.setScreen(t);
    CHECK(display.lines() == std::vector<std::string>{"ab\xE2\x80\xA6"});
    CHECK(t.scroll() == 0);

    display.reset();
    tui.loop(0);
    CHECK(display.lines().empty());
    tui.loop(400);
    CHECK(display.lines().empty());
    CHECK(t.scroll() == 0);

    tui.loop(500);
    CHECK(t.scroll() == 1);
    CHECK(display.lines() == std::vector<std::string>{"bc\xE2\x80\xA6"});

    display.reset();
    tui.loop(600);
    CHECK(display.lines().empty());
    CHECK(t.scroll() == 1);
}

TEST_CASE("Screen forwards measure, layout and render to its root") {
    stui::Column<stui::Text, stui::Text> root{stui::Text{"hi"}, stui::Text{"yo"}};
    stui::Screen screen{root};

    screen.measure(5, 2);
    screen.layout(0, 0, 5, 2);

    TestDisplay display;
    screen.render(display);

    const std::vector<std::string> expected = {"hi", "yo"};
    CHECK(display.lines() == expected);
}

TEST_CASE("SimpleTui::setScreen lays out and renders a screen") {
    TestDisplay display;
    stui::SimpleTui<5, 2> tui(display);

    stui::Column<stui::Text, stui::Text> root{stui::Text{"hi"}, stui::Text{"yo"}};
    stui::Screen screen{root};

    tui.setScreen(screen);

    const std::vector<std::string> expected = {"hi", "yo"};
    CHECK(display.lines() == expected);
}

TEST_CASE("SimpleTui constructor clears the display") {
    TestDisplay display;
    CHECK(display.clear_count == 0);

    stui::SimpleTui<10, 5> tui(display);

    CHECK(display.clear_count == 1);
}

TEST_CASE("SimpleTui::drawRect draws a rectangle") {
    TestDisplay display;
    stui::SimpleTui<7, 5> tui(display);

    tui.drawRect(0, 0, 7, 5);

    const std::vector<std::string> expected = {
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90",
        "\xE2\x94\x82     \xE2\x94\x82",
        "\xE2\x94\x82     \xE2\x94\x82",
        "\xE2\x94\x82     \xE2\x94\x82",
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98",
    };
    CHECK(display.lines() == expected);
}
