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
    char distance[12];

    const auto root = Panel{
      Text{"Main motors are disabled by safety switch"},
      Row{
        Column{
          Panel{
            Text{"battery: 3.9V"},
            Text{"WIFI: -66 dBm"}
          },
          Panel{
            Text{"x: 0"},
            Text{"y: 0"},
            // Text{"z: 0"}
          },
        },
        Panel{
          Text{distance},
          Text{"tempr: 10"},
          Text{"press: 1013 hPa"},
          Text{"humidity: 50%"},
          Text{"UV: normal"},
          Text{"uptime: 1234 s"}
        }
      }
    };

    tui.setScreen(root);

    std::atomic<bool> running{true};
    std::thread worker([&] {
        while (running.load()) {
            snprintf(distance, sizeof(distance), "dist: %d mm", counter);
            // tui.loop(now_ms());
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(177));
        }
    });

    worker.join();
    return 0;
}
