# simple-tui

A header-only C++ library for building simple text UI / terminal-style output,
intended to run on an Arduino (MCU) while being developed and tested on the
desktop.

The library is a single header (`include/simple_tui.hpp`) with no desktop-only
dependencies (`<iostream>`, heavy STL, etc.), so the same code compiles for both
the host tests and the microcontroller.

## Structure

```
simple-tui/
├── CMakeLists.txt              # project config: INTERFACE lib, demo binary, tests
├── include/
│   └── simple_tui.hpp          # the header-only library (Arduino-safe)
├── src/
│   ├── main.cpp                # empty main() for manual behaviour checks
│   └── terminal_display.hpp    # ANSI/stdout Display backend for the demo
├── tests/
│   ├── CMakeLists.txt          # pulls in doctest via FetchContent
│   └── test_simple_tui.cpp     # unit tests (parameterized + mock Display)
└── .gitignore
```

### Components

- **`include/simple_tui.hpp`** — the library (namespace `stui`).
  - `Display` — pure interface (`void clear()`, `void setColor(Color)` and
    `void print(uint16_t x, uint16_t y, const char* str)`) used as an output
    backend (LCD, OLED, Serial, terminal, ...). `clear()` wipes the whole
    screen; `SimpleTui` calls it from its constructor. `setColor` defaults to
    a no-op so monochrome backends stay compatible. `print` draws one cell
    at a time; multi-byte UTF-8 strings (box-drawing characters) are passed
    through whole.
  - **Glyph constants** — `glyph_top_left`, `glyph_horizontal`,
    `glyph_bottom_right`, `glyph_ellipsis`, etc. hold the UTF-8 box-drawing
    characters as escaped bytes (pure-ASCII source) with a comment naming each
    character and codepoint. Widgets use them instead of inline escapes.
  - **Colors** — `Color{r, g, b}` (24-bit RGB) with `color_white` and
    `color_green` constants. `Text` takes a color as its second argument,
    defaulting to white.
  - `Widget` — abstract base for the declarative tree. Every widget implements
    `measure(avail_w, avail_h)`, `layout(x, y, w, h)` and `render(Display&)`;
    all three are `const` and use `mutable` state, so a `const Widget&` can
    still be measured, laid out and animated (properties stay live).
  - `Text` — a leaf that measures to `strlen × 1`, renders char-by-char in its
    `Color` (second constructor argument, defaults to `color_white`), elides
    with `…` when wider than its box, and supports `setScroll(offset)`.
    Scrolling loops: once the tail reaches the window's end, the text continues
    with a three-space gap followed by the beginning of the string.
  - `Column` / `Row` — containers storing children **by value** in
    `std::tuple<const Children...>` (no heap). `Column` stacks vertically
    (`max width × sum height`), `Row` places horizontally at a fixed `spacing`
    (total width = sum of child widths + spacing between neighbours, height =
    max child height). `Row` takes the spacing as an optional first argument
    (`Row{2, Text{"a"}, Text{"b"}}`, default `0`). Both containers clamp their
    measured extent to the available space and shrink oversized children
    proportionally, so fill widgets like `Panel` stay on screen. `Column`
    allocates rows sequentially and gives every non-empty child at least one
    row, so a one-line `Text` before fill panels is not rounded away.
  - `Panel` — a bordered container that **fills its parent** and stacks its
    children vertically inside the border. Children are allocated within the
    interior height (above the bottom border), shrinking fill children as
    needed. Use it as an outer frame around other widgets.
  - `Screen` — wraps a `const Widget&` root and forwards the three passes. No
    template arguments needed, since screens are always handled polymorphically.
  - `SimpleTui<Width, Height>` — render-only entry point. `setScreen(Widget&)`
    (lvalues only) measures and lays the screen into the terminal, stores a
    pointer to it, and renders it.
    `loop(now_ms)` is the per-iteration hook: it redraws the current screen only
    once at least `redraw_interval_ms` (500 ms) have elapsed, calling each
    widget's `tick()` first so animations advance. `drawRect(...)` draws a box.
- **`src/main.cpp`** — a small runnable binary with a `StdoutDisplay`, building
  a `Screen { Panel { Text, Text } }` tree.
- **`src/terminal_display.hpp`** — desktop-only `Display` for the demo.
  `clear()` emits `ESC[2J ESC[H` (erase screen, home cursor); each
  `print(x, y, str)` emits an ANSI cursor-position sequence (`ESC[row;colH`,
  1-based) and writes only that string, so the whole screen is never repainted.
- **`tests/test_simple_tui.cpp`** — doctest unit tests with a `TestDisplay`
  grid mock that records every printed cell.

### Declarative layout

```cpp
SimpleTui<10, 5> tui(display);

auto root = Panel{Text{"hello"}, Text{"world"}};
tui.setScreen(root);
```

`setScreen` accepts **lvalues only**. The screen is stored by pointer, so a
temporary (`tui.setScreen(Panel{...})`) would dangle once the call returns;
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
./build/simple_tui_run
```

To skip tests (e.g. a lean build):

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
```

To skip `compile_commands.json` generation:

```bash
cmake -S . -B build -DSIMPLE_TUI_EXPORT_COMPILE_COMMANDS=OFF
```

## Using on Arduino

The library is header-only: copy (or symlink) `include/simple_tui.hpp` into your
Arduino sketch's folder and `#include "simple_tui.hpp"`. Implement `Display`
for your hardware and pass it to the `SimpleTui` constructor.

For now it expect to be able to print at least this characters,
make sure your font support them

```text
|\─│┌┐└┘├┤┬┴┼
```

I've use a font created by <https://fonts.atomic14.com/> with the chars:

```text
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789+-=*/%<>±×.,:;!?^~_()[]{}‹›«»@#$&|\─│┌┐└┘├┤┬┴┼
```
