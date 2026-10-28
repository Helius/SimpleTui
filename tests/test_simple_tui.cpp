// test_simple_tui.cpp — tests for the header-only library.
//
// Pattern: a data table (input + expected result) iterated with SUBCASE,
// so doctest reports each row individually when it fails.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <simple_tui.hpp>
#include <stdint.h>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Mock Display that records every print() call.
// ---------------------------------------------------------------------------
class TestDisplay : public simple_tui::Display {
public:
    void print(uint16_t x, uint16_t y, const char* str) const override {
        last_x = x;
        last_y = y;
        last_str = str ? str : "";
        call_count++;
    }

    void clearScreen() const override {
        clear_count++;
    }

    // mutable so const print() can write — test inspection only.
    mutable uint16_t last_x = 0;
    mutable uint16_t last_y = 0;
    mutable std::string last_str;
    mutable int call_count = 0;
    mutable int clear_count = 0;
};

// ---------------------------------------------------------------------------
// SimpleTui — template dimensions
// ---------------------------------------------------------------------------

TEST_CASE("SimpleTui<80,24>: getters return template dimensions") {
    TestDisplay display;
    simple_tui::SimpleTui<80, 24> tui(display);
    CHECK(tui.width()  == 80);
    CHECK(tui.height() == 24);
}

// ---------------------------------------------------------------------------
// SimpleTui — constructor clears the display
// ---------------------------------------------------------------------------

TEST_CASE("SimpleTui: constructor calls Display::clearScreen") {
    TestDisplay display;
    CHECK(display.clear_count == 0);

    simple_tui::SimpleTui<80, 24> tui(display);

    CHECK(display.clear_count == 1);
}

// Compile-time check.
static_assert(simple_tui::SimpleTui<80, 24>::width()  == 80, "width compile-time");
static_assert(simple_tui::SimpleTui<80, 24>::height() == 24, "height compile-time");

