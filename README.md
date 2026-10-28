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
│   └── main.cpp                # empty main() for manual behaviour checks
├── tests/
│   ├── CMakeLists.txt          # pulls in doctest via FetchContent
│   └── test_simple_tui.cpp     # unit tests (parameterized + mock Display)
└── .gitignore
```

### Components

- **`include/simple_tui.hpp`** — the library.
  - `Display` — pure interface (`void print(uint16_t x, uint16_t y, const char* str)`)
    used as an output backend (LCD, OLED, Serial, terminal, ...). It prints one
    cell at a time; multi-byte UTF-8 strings (box-drawing characters) are passed
    through whole.
  - `SimpleTui<Width, Height>` — dimensions are compile-time template
    parameters; holds a `const Display&` injected via the constructor. The
    constructor draws a border via `drawRect(0, 0, Width, Height)`, and
    `drawRect(x, y, width, height)` can draw further rectangles anywhere.
    `print` forwards a cell to the backend.
- **`src/main.cpp`** — a small runnable binary with a stub `Display`, useful for
  poking at library behaviour by hand.
- **`tests/test_simple_tui.cpp`** — doctest unit tests, including a `TestDisplay`
  mock that records calls, and a data-table parameterized test pattern.

## Testing

Tests use [doctest](https://github.com/doctest/doctest) — a single-header,
lightweight alternative to Google Test, fetched automatically by CMake.

Parameterized tests use a data table iterated with `SUBCASE`, so doctest reports
each row individually when it fails.

## Build & run

```bash
# configure (tests enabled by default)
cmake -S . -B build

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
