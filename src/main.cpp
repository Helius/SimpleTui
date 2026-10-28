#include <simple_tui.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace stui;

class StdoutDisplay : public Display {
public:
    void print(uint16_t x, uint16_t y, const char* str) const override {
        if (buffer.size() <= y)
            buffer.resize(y + 1);
        auto& row = buffer[y];
        if (row.size() <= x)
            row.resize(x + 1, " ");
        row[x] = (str != nullptr) ? str : "";
    }

    void dump() const {
        for (const auto& row : buffer) {
            std::string s;
            for (const auto& cell : row)
                s += cell;
            std::printf("%s\n", s.c_str());
        }
    }

    mutable std::vector<std::vector<std::string>> buffer;
};

int main() {
    StdoutDisplay display;
    SimpleTui<10, 5> tui(display);

    const auto screen = Panel
    {
      Text{"hello"}, 
      Text{"world"},
      Text{"very long string that should be scrolled"}
    }; 

    tui.setScreen(screen);

    display.dump();
    return 0;
}
