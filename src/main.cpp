#include <simple_tui.hpp>

#include "terminal_display.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace stui;

static uint32_t now_ms() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

int main() {
    StdoutDisplay display;
    SimpleTui<10, 5> tui(display);

    static Panel<Text, Text> root{
        Text{"hello scrolling world"},
        Text{"second line"}};

    tui.setScreen(root);

    std::atomic<bool> running{true};
    std::thread worker([&] {
        while (running.load()) {
            tui.loop(now_ms());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2600));
    worker.join();

    return 0;
}
