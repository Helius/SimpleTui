# SimpleTui

A header-only C++ library for building simple text UI / terminal-style output,
intended to run on an Arduino (MCU) while being developed and tested on the
desktop.

The library is a single header (`SimpleTui.h`) with no desktop-only
dependencies (`<iostream>`, heavy STL, etc.), so the same code compiles for both
the host tests and the microcontroller. The header lives in the project root to
follow the Arduino library convention: `#include <SimpleTui.h>`.

## Structure

```
SimpleTui/
├── CMakeLists.txt              # project config: INTERFACE lib, demo binary, tests
├── SimpleTui.h                 # the header-only library (Arduino-safe)
├── src/
│   ├── main.cpp                # runnable demo / manual behaviour checks
│   └── terminal_display.hpp    # ANSI/stdout Display backend for the demo
├── examples/
│   └── esp32c3_mock/           # heap-free Arduino mock Display sketch
├── tests/
│   ├── CMakeLists.txt          # pulls in doctest via FetchContent
│   └── test_simple_tui.cpp     # unit tests (parameterized + mock Display)
└── .gitignore
```

### Components

- **`SimpleTui.h`** — the library (namespace `stui`).
  - `Display` — pure interface (`void clear()`, `void setColor(Color)`,
    `void setColors(const Colors&)` and
    `void print(uint16_t x, uint16_t y, const char* str)`) used as an output
    backend (LCD, OLED, Serial, terminal, ...). `clear()` wipes the whole
    screen; `SimpleTui` calls it from its constructor. `setColor`/`setColors`
    default to no-ops (the latter forwarding to the foreground color) so
    monochrome backends stay compatible. `print` draws one cell
    at a time; multi-byte UTF-8 strings (box-drawing characters) are passed
    through whole.
  - **Glyph constants** — `glyph_top_left`, `glyph_horizontal`,
    `glyph_bottom_right`, `glyph_ellipsis`, etc. hold the UTF-8 box-drawing
    characters as escaped bytes (pure-ASCII source) with a comment naming each
    character and codepoint. Widgets use them instead of inline escapes.
  - **Colors** — `Color{r, g, b}` (24-bit RGB, with `color_black`,
    `color_white`, `color_green` constants), `Colors{bg, fg}`, and
    `TextConfig{color, border = false}`. `Text` takes a `TextConfig` as its
    second argument, defaulting to white on black and no border:
    ```cpp
    Text{"dist", {.color = {.fg = color_green}}}
    Text{"boxed", {.border = true}}
    ```
    `TextConfig` is the place to add further per-text settings later.
  - `Widget` — abstract base for the declarative tree. Every widget implements
    `measure(avail_w, avail_h)`, `layout(x, y, w, h)` and `render(Display&)`;
    all three are `const` and use `mutable` state, so a `const Widget&` can
    still be measured, laid out and animated (properties stay live).
  - `Text` — a leaf that measures to `strlen × 1` (or `(strlen + 2) × 3` with
    `border`), renders char-by-char with its `TextConfig` colors (default white
    on black), elides with `…` when wider than its box, and supports
    `setScroll(offset)`. With `border`, the rectangle is drawn and the text
    window shrinks to the interior.
    Scrolling loops: once the tail reaches the window's end, the text continues
    with a three-space gap followed by the beginning of the string.
  - `Column` / `Row` — containers storing children **by value** in
    `std::tuple<const Children...>` (no heap). `Column` stacks vertically
    (`max width × sum height + spacing`), `Row` places horizontally
    (total width = sum of child widths + `spacing` between neighbours, height =
    max child height). Both clamp their measured extent to the available space
    and shrink oversized children proportionally. `Column` allocates rows
    sequentially and gives every non-empty child at least one row, so small
    content is not rounded away.
  - **`LayoutConfig`** — `LayoutConfig{border = false, spacing = 0, color = {}}`
    is accepted as an optional first argument by `Row` and `Column`. `border`
    draws a rectangle with the children laid out inside it (the measured extent
    grows by the border); `spacing` is the gap between neighbours (horizontal
    for `Row`, vertical for `Column`); `color` is the foreground/background
    color used to draw that border. Because the argument type is fixed, CTAD
    still deduces the child types, so no template arguments are needed:
    ```cpp
    Row{{.border = true}, Text{"a"}, Text{"b"}}
    Column{{.spacing = 1}, Text{"a"}, Text{"b"}}
    Row{{.border = true, .spacing = 2, .color = {.fg = color_green}},
        Text{"a"}, Text{"b"}}
    ```
    Designated initializers must follow declaration order (`border`, then
    `spacing`, then `color`) when setting several. `Text{}` default-constructs
    an empty string, so placeholder children work in this form.
  - `Screen` — wraps a `const Widget&` root and forwards the three passes. No
    template arguments needed, since screens are always handled polymorphically.
  - `SimpleTui<Width, Height>` — render-only entry point. `setScreen(Widget&)`
    (lvalues only) measures and lays the screen into the terminal, stores a
    pointer to it, and renders it.
    `loop(now_ms)` is the per-iteration hook: it redraws the current screen only
    once at least `redraw_interval_ms` (500 ms) have elapsed, calling each
    widget's `tick()` first so animations advance. `drawRect(...)` draws a box.
- **`src/main.cpp`** — a small runnable binary with a `StdoutDisplay`, building
  a nested `Column` / `Row` tree with borders.
- **`src/terminal_display.hpp`** — desktop-only `Display` for the demo.
  `clear()` emits `ESC[2J ESC[H` (erase screen, home cursor); each
  `print(x, y, str)` emits an ANSI cursor-position sequence (`ESC[row;colH`,
  1-based) and writes only that string, so the whole screen is never repainted.
- **`tests/test_simple_tui.cpp`** — doctest unit tests with a `TestDisplay`
  grid mock that records every printed cell.

### Declarative layout

```cpp
SimpleTui<10, 5> tui(display);

auto root = Column{{.border = true}, Text{"hello"}, Text{"world"}};
tui.setScreen(root);
```

`setScreen` accepts **lvalues only**. The screen is stored by pointer, so a
temporary (`tui.setScreen(Column{...})`) would dangle once the call returns;
passing one is rejected at compile time. Keep the root alive for as long as the
`SimpleTui` may redraw it (a local that outlives the render loop, a `static`,
or a global).

Rendering is a two-pass pipeline: `measure` computes desired sizes bottom-up,
`layout` assigns boxes top-down, then `render` draws. A separate `Navigator`
class (future work) will own the screen graph and drive `setScreen`.

### Driving updates

`loop(now_ms)` takes the current time in milliseconds so the library stays
platform-agnostic (use `millis()` on Arduino, `steady_clock` on desktop). Widgets
advance their animation in `tick()`; `Text` scrolls its content when it doesn't
fit its box. On the desktop the demo runs `loop` on a background thread:

```cpp
std::atomic<bool> running{true};
std::thread worker([&] {
    while (running.load()) {
        tui.loop(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
});
```

The screen passed to `setScreen` is stored by pointer, so it must outlive the
`SimpleTui` redraw loop (e.g. a local in the same scope, a `static`/global root).
Temporaries are rejected at compile time; a local that goes out of scope early
is still your responsibility.

## Testing

Tests use [doctest](https://github.com/doctest/doctest) — a single-header,
lightweight alternative to Google Test, fetched automatically by CMake.

Parameterized tests use a data table iterated with `SUBCASE`, so doctest reports
each row individually when it fails.

## Build & run

```bash
# configure (tests enabled by default)
cmake -S . -B build
# also writes build/compile_commands.json for clangd

# build everything
cmake --build build

# run tests
cd build && ctest --output-on-failure

# run the manual demo binary
./build/SimpleTuiRun
```

To skip tests (e.g. a lean build):

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
```

To skip `compile_commands.json` generation:

```bash
cmake -S . -B build -DSIMPLETUI_EXPORT_COMPILE_COMMANDS=OFF
```

## Using on Arduino

The library is header-only: copy (or symlink) `SimpleTui.h` into your Arduino
sketch's folder and `#include "SimpleTui.h"`. Implement `Display` for your
hardware and pass it to the `SimpleTui` constructor. See
`examples/esp32c3_mock/` for a minimal heap-free sketch.

For now it expect to be able to print at least this characters,
make sure your font support them

```text
|\─│┌┐└┘├┤┬┴┼
```

I've use a font created by <https://fonts.atomic14.com/> with the chars:

```text
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789+-=*/%<>±×.,:;!?^~_()[]{}‹›«»@#$&|\─│┌┐└┘├┤┬┴┼
```
