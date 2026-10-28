#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <simple_tui.hpp>
#include <stdint.h>

#include <string>
#include <vector>

class TestDisplay : public stui::Display {
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

TEST_CASE("Row measure is sum width by max height") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    CHECK(row.measured_width() == 4);
    CHECK(row.measured_height() == 1);
}

TEST_CASE("Row places children horizontally") {
    stui::Row<stui::Text, stui::Text> row{stui::Text{"ab"}, stui::Text{"cd"}};
    row.measure(80, 24);
    row.layout(0, 0, 80, 1);

    TestDisplay display;
    row.render(display);

    const std::vector<std::string> expected = {"abcd"};
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
