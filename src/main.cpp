#include <simple_tui.hpp>

#include <cstdio>
#include <string>
#include <vector>

class StdoutDisplay : public simple_tui::Display {
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
    simple_tui::SimpleTui<10, 5> tui(display);

    tui.print(2, 2, "X");
    display.dump();
    return 0;
}