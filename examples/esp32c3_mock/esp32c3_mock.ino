// Minimal Arduino example: drive simple-tui with an in-memory Display mock.
// Compile for an ESP32-C3 from the repo root with:
//   arduino-cli compile --fqbn esp32:esp32:esp32c3 \
//       --build-property "compiler.cpp.extra_flags=-I$PWD/include" \
//       examples/esp32c3_mock
//
// Swap MockDisplay for a real LCD/OLED/Serial backend implementing the same
// three Display methods; the widget tree stays unchanged.

#include <Arduino.h>
#include <simple_tui.hpp>

class MockDisplay : public stui::Display {
public:
    static constexpr uint16_t kWidth = 24;
    static constexpr uint16_t kHeight = 6;

    void clear() const override {
        for (uint16_t y = 0; y < kHeight; ++y)
            for (uint16_t x = 0; x < kWidth; ++x)
                cells[y][x] = ' ';
    }

    void setColors(const stui::Colors&) const override {}

    void print(uint16_t x, uint16_t y, const char* str) const override {
        if (str == nullptr || x >= kWidth || y >= kHeight)
            return;
        cells[y][x] = str[0]; // one cell per call; UTF-8 glyphs are multi-byte
        ++printed;
    }

    mutable char cells[kHeight][kWidth] = {};
    mutable uint32_t printed = 0;
};

static MockDisplay display;
static stui::SimpleTui<MockDisplay::kWidth, MockDisplay::kHeight> tui(display);

static const auto root = stui::Column{
    {.border = true},
    stui::Text{"Hello, ESP32-C3!"},
    stui::Row{
        {.spacing = 1},
        stui::Text{"a"},
        stui::Text{"b", {.border = true}},
    },
};

void setup() {
    Serial.begin(115200);
    tui.setScreen(root);
    Serial.printf("simple-tui rendered %lu cells\n",
                  static_cast<unsigned long>(display.printed));
}

void loop() {
    tui.loop(millis());
}