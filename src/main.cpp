// main.cpp — manual behaviour check.
// Use this to drive the header-only library interactively while developing.
// Not built/used on Arduino.

#include <simple_tui.hpp>

// Minimal stub display so the binary links and runs.
class StubDisplay : public simple_tui::Display {
public:
    void clearScreen() const override {}
    void print(uint16_t, uint16_t, const char*) const override {}
};

int main() {
    StubDisplay display;
    simple_tui::SimpleTui<80, 24> tui(display);
    display.print(0, 0, "hello");
    return 0;
}
