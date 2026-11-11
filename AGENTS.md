# AGENTS.md

`SimpleTui` is a header-only C++17 text-UI library, developed/tested on the
desktop but intended to compile and run on an Arduino (AVR/ESP) microcontroller.

Read `README.md` for the component overview, then `SimpleTui.h` before changing
layout or rendering behavior.

## Layout

- `SimpleTui.h` — the whole library (namespace `stui`). Arduino-safe.
- `src/terminal_display.hpp` — desktop-only `Display` backend (`<iostream>`, ANSI).
- `src/main.cpp` — user-owned demo binary. Do not modify without explicit intent.
- `tests/test_simple_tui.cpp` — doctest suite with a `TestDisplay` grid mock.

## Library constraints (important)

The library header must stay compilable on an Arduino:

- **Header-only**, no `.cpp`, no linking.
- **No heap**: widgets store children by value (`std::tuple<const Children...>`),
  no `new`/`delete`/`malloc`/`std::vector`/`std::string`.
- **No `<iostream>`**, no desktop I/O, no exceptions/RTTI assumptions.
- Only minimal standard headers: `<stdint.h>`, `<string.h>`, `<tuple>`,
  `<type_traits>` (C++17 `std::apply`/`std::tuple` are used).
- Output only through the `Display` interface (`clear`, `setColor`/`setColors`,
  `print`). Platform code implements that interface outside the library.
- Source is pure ASCII: box-drawing glyphs live in escaped-byte constants
  (`glyph_*`) with codepoint comments.

Desktop-only code belongs in `src/`, never in the library header.

## Conventions

- `measure(avail_w, avail_h)` sets desired size (`mw_/mh_`) and minimum size
  (`min_w_/min_h_`); `layout(x,y,w,h)` assigns the real box; measured size
  never moves a widget.
- `Row`/`Column` layout is greedy: each child gets its full measured size
  while later children can still fit at their `min_*`, else as much as fits.
  `Text` skips rendering when its box has no content row (`h_` too small).
- `Row`/`Column` accept an optional first `LayoutConfig{border, spacing}` via
  CTAD. `Text` accepts a `TextConfig{color, border}`.
- `SimpleTui::setScreen` takes lvalues only (the screen is stored by pointer).
- Designated initializers must follow declaration order.
- New behavior is TDD: write a failing test first, then implement.

## Build & test

```bash
cmake -S . -B build          # writes build/compile_commands.json for clangd
cmake --build build
ctest --test-dir build --output-on-failure
./build/SimpleTuiRun         # desktop demo
```

### Arduino build check

The library is verified with `arduino-cli` against the ESP32 core. From the repo
root:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3 \
    --build-property "compiler.cpp.extra_flags=-I$PWD" \
    examples/esp32c3_mock
```

`examples/esp32c3_mock/esp32c3_mock.ino` is a heap-free mock `Display` sketch.
If the core is missing, install it with `arduino-cli core install esp32:esp32`.