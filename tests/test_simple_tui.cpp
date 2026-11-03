#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <SimpleTui.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>
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

// Detects whether `tui.setScreen(arg)` is well-formed, without instantiating
// the function body (so it reflects the overload/SFINAE constraints).
template <typename Tui, typename Arg, typename = void>
struct can_set_screen : std::false_type {};

template <typename Tui, typename Arg>
struct can_set_screen<Tui, Arg, std::void_t<decltype(
    std::declval<Tui&>().setScreen(std::declval<Arg>()))>> : std::true_type {};

// ---------------------------------------------------------------------------
// StdoutDisplay — ANSI output backend
// ---------------------------------------------------------------------------

TEST_CASE("StdoutDisplay::clear wipes the terminal and homes the cursor") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.clear();

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[0m\x1b[2J\x1b[H");
}

TEST_CASE("StdoutDisplay::setColor emits a 24-bit foreground color") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.setColor(stui::Color{1, 2, 3});

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[38;2;1;2;3m");
}

TEST_CASE("StdoutDisplay::setColors emits 24-bit foreground and background") {
    StdoutDisplay display;
    std::ostringstream out;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());

    display.setColors(stui::Colors{.fg = stui::Color{1, 2, 3},
                                   .bg = stui::Color{4, 5, 6}});

    std::cout.rdbuf(old);
    CHECK(out.str() == "\x1b[38;2;1;2;3m\x1b[48;2;4;5;6m");
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

TEST_CASE("Text default-constructs as an empty string") {
    stui::Text t{};
    t.measure(3, 1);
    CHECK(t.measured_width() == 0);
    CHECK(t.measured_height() == 1);
}

TEST_CASE("Text renders with its TextConfig colors and restores defaults") {
    class ColorDisplay : public stui::Display {
    public:
        void clear() const override {}
        void setColors(const stui::Colors& c) const override { current = c; }
        void print(uint16_t, uint16_t, const char*) const override { printed = current; }

        mutable stui::Colors current{};
        mutable stui::Colors printed{};
    };

    stui::Text green{"hi", {.color = {.fg = stui::color_green}}};
    green.measure(5, 1);
    green.layout(0, 0, 5, 1);

    ColorDisplay display;
    green.render(display);
    CHECK(display.printed.fg == stui::color_green);
    CHECK(display.printed.bg == stui::color_black);
    CHECK(display.current == stui::Colors{}); // reset for the next widget

    stui::Text def{"hi"};
    def.measure(5, 1);
    def.layout(0, 0, 5, 1);

    ColorDisplay display2;
    def.render(display2);
    CHECK(display2.printed.fg == stui::color_white);
    CHECK(display2.printed.bg == stui::color_black);
}

TEST_CASE("Text with a border measures and renders its text inside") {
    stui::Text t{"hi", {.border = true}};
    t.measure(10, 10);
    CHECK(t.measured_width() == 4);  // "hi" + two border columns
    CHECK(t.measured_height() == 3); // one content row + two border rows

    t.layout(0, 0, 4, 3);

    TestDisplay display;
    t.render(display);

    const std::vector<std::string> expected = {
        "┌──┐",
        "│hi│",
        "└──┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Text with a border elides inside the interior width") {
    stui::Text t{"hello", {.border = true}};
    t.measure(5, 3); // clamped to the available width
    CHECK(t.measured_width() == 5);
    CHECK(t.measured_height() == 3);

    t.layout(0, 0, 5, 3);

    TestDisplay display;
    t.render(display);

    const std::vector<std::string> expected = {
        "┌───┐",
        "│he…│",
        "└───┘",
    };
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

TEST_CASE("Column stacks bordered columns at their own heights") {
    stui::Column col{
        stui::Column{{.border = true},
                     stui::Text{"a"}, stui::Text{"b"}, stui::Text{"c"}},
        stui::Column{{.border = true}, stui::Text{"d"}}};

    col.measure(16, 12);
    // (3 content rows + 2 border) + (1 content row + 2 border)
    CHECK(col.measured_height() == 8);

    col.layout(0, 0, 16, 12);

    TestDisplay display;
    col.render(display);

    const std::vector<std::string> expected = {
        "┌──────────────┐",
        "│a             │",
        "│b             │",
        "│c             │",
        "└──────────────┘",
        "┌──────────────┐",
        "│d             │",
        "└──────────────┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Column keeps content children visible when fill children would starve them") {
    stui::Column col{
        stui::Text{"hi"},
        stui::Column{{.border = true}, stui::Text{"a"}},
        stui::Column{{.border = true}, stui::Text{"b"}}};

    col.measure(16, 9);
    col.layout(0, 0, 16, 9);

    TestDisplay display;
    col.render(display);

    // Text 1 row, then each panel wraps to 1 content row + 2 border rows.
    const std::vector<std::string> expected = {
        "hi",
        "┌──────────────┐",
        "│a             │",
        "└──────────────┘",
        "┌──────────────┐",
        "│b             │",
        "└──────────────┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Column measures bordered columns by their content") {
    stui::Column col{
        stui::Column{{.border = true}, stui::Text{"a"}},
        stui::Column{{.border = true}, stui::Text{"b"}}};

    col.measure(5, 6);
    CHECK(col.measured_width() == 3);  // "a" + two border columns
    CHECK(col.measured_height() == 6); // (1 + 2) * 2

    col.layout(0, 0, 3, 6);

    TestDisplay display;
    col.render(display);

    const std::vector<std::string> expected = {
        "┌─┐",
        "│a│",
        "└─┘",
        "┌─┐",
        "│b│",
        "└─┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Row measure is sum width plus spacing by max height") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    CHECK(row.measured_width() == 4); // 2 + 0 (default spacing) + 2
    CHECK(row.measured_height() == 1);
}

TEST_CASE("Row places children horizontally with no space by default") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    row.layout(0, 0, 80, 1);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {"abcd"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Row custom spacing separates children") {
    stui::Row row{{.spacing = 2}, stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    CHECK(row.measured_width() == 6); // 2 + 2 (spacing) + 2
    row.layout(0, 0, 6, 1);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {"ab  cd"};
    CHECK(display.lines() == expected);
}

TEST_CASE("Column custom spacing separates children vertically") {
    stui::Column col{
        {.spacing = 1},
        stui::Column{{.border = true}, stui::Text{"a"}},
        stui::Column{{.border = true}, stui::Text{"b"}}};

    col.measure(3, 10);
    CHECK(col.measured_height() == 7); // (1 + 2) + 1 spacing + (1 + 2)

    col.layout(0, 0, 3, 7);

    TestDisplay display;
    col.render(display);

    const std::vector<std::string> expected = {
        "┌─┐",
        "│a│",
        "└─┘",
        "",
        "┌─┐",
        "│b│",
        "└─┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Row places Texts back-to-back") {
    stui::Row<stui::Text, stui::Text, stui::Text> row{
        stui::Text{"dist: "}, stui::Text{"10"}, stui::Text{" m"}};

    row.measure(14, 1);
    row.layout(0, 0, 14, 1);

    TestDisplay display;
    row.render(display);

    CHECK(display.lines() == std::vector<std::string>{"dist: 10 m"});
}

TEST_CASE("Row repositions children when a Text buffer grows") {
    char value[4] = "0";
    stui::Row<stui::Text, stui::Text, stui::Text> row{
        stui::Text{"dist: "}, stui::Text{value}, stui::Text{" m"}};

    row.measure(14, 1);
    row.layout(0, 0, 14, 1);

    value[0] = '1';
    value[1] = '0';
    value[2] = '\0';

    TestDisplay display;
    row.render(display);

    CHECK(display.lines() == std::vector<std::string>{"dist: 10 m"});
}

TEST_CASE("Row accepts a LayoutConfig first argument via CTAD and draws a border") {
    stui::Row row{{.border = true}, stui::Text{"ab"}, stui::Text{"cd"}};
    static_assert(
        std::is_same_v<decltype(row), stui::Row<stui::Text, stui::Text>>);

    row.measure(10, 3);
    CHECK(row.measured_width() == 6);  // "abcd" + two border columns
    CHECK(row.measured_height() == 3); // one content row + two border rows

    row.layout(0, 0, 6, 3);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {
        "┌────┐",
        "│abcd│",
        "└────┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Row renders its border with the LayoutConfig color") {
    class RecordingDisplay : public stui::Display {
    public:
        struct Draw {
            uint16_t x;
            uint16_t y;
            std::string str;
            stui::Colors colors;
        };

        void clear() const override {}
        void setColors(const stui::Colors& c) const override { current = c; }
        void print(uint16_t x, uint16_t y, const char* str) const override {
            draws.push_back({x, y, str != nullptr ? str : "", current});
        }

        mutable stui::Colors current{};
        mutable std::vector<Draw> draws;
    };

    const stui::Colors borderColors{.fg = stui::color_green,
                                    .bg = stui::color_blue};
    stui::Row row{
        {.border = true, .color = borderColors},
        stui::Text{"ab"},
        stui::Text{"cd"}};
    row.measure(10, 3);
    row.layout(0, 0, 6, 3);

    RecordingDisplay display;
    row.render(display);

    REQUIRE(display.draws.size() > 6);
    CHECK(display.draws.front().colors == borderColors);
    CHECK(display.draws[5].colors == borderColors); // top-right corner
    for (const auto& draw : display.draws) {
        if (draw.str == "a" || draw.str == "b" || draw.str == "c" ||
            draw.str == "d") {
            CHECK(draw.colors == stui::Colors{});
        }
    }
}

TEST_CASE("Column renders its border with the LayoutConfig color") {
    class RecordingDisplay : public stui::Display {
    public:
        struct Draw {
            uint16_t x;
            uint16_t y;
            std::string str;
            stui::Colors colors;
        };

        void clear() const override {}
        void setColors(const stui::Colors& c) const override { current = c; }
        void print(uint16_t x, uint16_t y, const char* str) const override {
            draws.push_back({x, y, str != nullptr ? str : "", current});
        }

        mutable stui::Colors current{};
        mutable std::vector<Draw> draws;
    };

    const stui::Colors borderColors{.fg = stui::color_yellow,
                                    .bg = stui::color_blue};
    stui::Column col{
        {.border = true, .color = borderColors},
        stui::Text{"ab"},
        stui::Text{"cd"}};
    col.measure(6, 10);
    col.layout(0, 0, 4, 4);

    RecordingDisplay display;
    col.render(display);

    REQUIRE_FALSE(display.draws.empty());
    CHECK(display.draws.front().colors == borderColors);
    CHECK(display.draws[3].colors == borderColors);
    for (const auto& draw : display.draws) {
        if (draw.str == "a" || draw.str == "b" || draw.str == "c" ||
            draw.str == "d") {
            CHECK(draw.colors == stui::Colors{});
        }
    }
}

TEST_CASE("Column accepts a LayoutConfig first argument via CTAD and draws a border") {
    stui::Column col{{.border = true}, stui::Text{"ab"}, stui::Text{"cd"}};
    static_assert(std::is_same_v<decltype(col),
                                 stui::Column<stui::Text, stui::Text>>);

    col.measure(6, 10);
    CHECK(col.measured_width() == 4);  // "ab"/"cd" + two border columns
    CHECK(col.measured_height() == 4); // two content rows + two border rows

    col.layout(0, 0, 4, 4);

    TestDisplay display;
    col.render(display);

    const std::vector<std::string> expected = {
        "┌──┐",
        "│ab│",
        "│cd│",
        "└──┘",
    };
    CHECK(display.lines() == expected);
}

TEST_CASE("Row shrinks fill children so it never exceeds its assigned width") {
    stui::Row row{
        {.spacing = 4},
        stui::Column{{.border = true}, stui::Text{"a"}},
        stui::Column{{.border = true}, stui::Text{"b"}}};

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

TEST_CASE("Bordered Column fits children inside its interior, above the bottom border") {
    stui::Column outer{
        {.border = true},
        stui::Text{"title"},
        stui::Column{{.border = true}, stui::Text{"abc"}}};

    // Interior is 8x4, but the children measure 1 + 3 = 4 rows: the inner
    // column must share the height instead of covering the outer bottom border.
    outer.measure(10, 6);
    outer.layout(0, 0, 10, 6);

    TestDisplay display;
    outer.render(display);

    const std::vector<std::string> expected = {
        "┌────────┐",
        "│title   │",
        "│┌──────┐│",
        "││abc   ││",
        "│└──────┘│",
        "└────────┘",
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

TEST_CASE("SimpleTui::setScreen accepts lvalues and rejects rvalues") {
    using Tui = stui::SimpleTui<5, 2>;

    CHECK(can_set_screen<Tui, stui::Text&>::value);
    CHECK(can_set_screen<Tui, const stui::Text&>::value);
    CHECK_FALSE(can_set_screen<Tui, stui::Text>::value);
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
