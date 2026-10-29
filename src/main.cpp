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
    SimpleTui<34, 11> tui(display);
    int counter = 0;
    char number[5];

    const auto root = Panel{
      Row{
        Column{
          Panel{
            Text{"hello"},
            Text{"world"}
          },
          Panel{
            Text{"ttt"},
            Text{"xxx"}
          },
          Text{"very long string that doesn't fit"},
        },
        Panel{
          Text{number}
        }
      }
    };

    tui.setScreen(root);

    std::atomic<bool> running{true};
    std::thread worker([&] {
        while (running.load()) {
            snprintf(number, sizeof(number), "%d", counter);
            tui.loop(now_ms());
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2600));
    worker.join();

    return 0;
}
