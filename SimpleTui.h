#pragma once

#include <stdint.h>
#include <string.h>
#include <tuple>
#include <type_traits>

namespace stui {

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
};

constexpr bool operator==(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

constexpr bool operator!=(const Color& a, const Color& b) { return !(a == b); }

inline constexpr Color color_black{0, 0, 0};
inline constexpr Color color_white{255, 255, 255};
inline constexpr Color color_green{149, 255, 143};
inline constexpr Color color_red{0xFF, 0x60, 0x8F};
inline constexpr Color color_yellow{255, 255, 0};
inline constexpr Color color_blue{0x1A, 0x53, 0x99};

struct Colors {
    Color fg = color_white;
    Color bg = color_black;
};

constexpr bool operator==(const Colors& a, const Colors& b) {
    return a.bg == b.bg && a.fg == b.fg;
}

constexpr bool operator!=(const Colors& a, const Colors& b) { return !(a == b); }

// Color storage policies for EBO-based optional color (see Text, Row, Column).
// NoColor is an empty base — zero bytes via Empty Base Optimization.
// WithColor stores a Colors value; pass it as the first "decoration" argument.
struct NoColor {};
struct WithColor { Colors value; };

struct TextConfig {
    bool border = false;
};

// Shared by layout containers (Row, Column). Pass it as the first argument to
// opt in; the default keeps the previous borderless behaviour.
struct LayoutConfig {
    bool border = false;
    uint16_t spacing = 0;
};

class Display {
public:
    virtual ~Display() = default;
    virtual void clear() const = 0;
    // Monochrome backends can ignore this; the default keeps them compatible.
    virtual void setColor(Color) const {}
    virtual void setColors(const Colors& colors) const { setColor(colors.fg); }
    virtual void print(uint16_t x, uint16_t y, const char* str) const = 0;
};

// UTF-8 glyphs used by the widgets. Kept as byte escapes so this header
// stays pure ASCII (safe on any editor/toolchain); each comment names the
// character and codepoint those bytes encode.
inline constexpr const char* glyph_top_left     = "\xE2\x94\x8C"; // ┌ U+250C
inline constexpr const char* glyph_top_right    = "\xE2\x94\x90"; // ┐ U+2510
inline constexpr const char* glyph_bottom_left  = "\xE2\x94\x94"; // └ U+2514
inline constexpr const char* glyph_bottom_right = "\xE2\x94\x98"; // ┘ U+2518
inline constexpr const char* glyph_horizontal   = "\xE2\x94\x80"; // ─ U+2500
inline constexpr const char* glyph_vertical     = "\xE2\x94\x82"; // │ U+2502
inline constexpr const char* glyph_ellipsis     = "\xE2\x80\xA6"; // … U+2026

inline uint16_t max_u16(uint16_t a, uint16_t b) { return a > b ? a : b; }

// Lay children out top-to-bottom in the given box. Children are visited in
// order; each gets its full measured height while every following child can
// still fit at its minimum height, otherwise it gets as much as fits (at
// least its own minimum). So earlier children keep their natural size and
// later ones shrink first, rather than every child shrinking proportionally.
template <typename Tuple>
inline void layout_vertically(const Tuple& children, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t spacing) {
    uint16_t count = 0, total_min = 0;
    std::apply([&](const auto&... c) {
        ((total_min = static_cast<uint16_t>(total_min + c.min_height()),
          ++count), ...);
    }, children);

    const uint16_t gaps = (count > 1) ? static_cast<uint16_t>(spacing * (count - 1)) : 0;
    const uint16_t avail = (h > gaps) ? static_cast<uint16_t>(h - gaps) : 0;

    uint16_t cy = y;
    uint16_t remaining = avail;
    uint16_t remaining_min = total_min;
    auto place = [&](const auto& c) {
        uint16_t desired = c.measured_height();
        uint16_t min_h = c.min_height();
        uint16_t min_rest = (remaining_min > min_h)
                                ? static_cast<uint16_t>(remaining_min - min_h) : 0;
        uint16_t max_for = (remaining > min_rest)
                                ? static_cast<uint16_t>(remaining - min_rest) : 0;
        uint16_t share = (desired <= max_for) ? desired : max_for;
        if (share < min_h) share = min_h;
        if (share > remaining) share = remaining;
        c.layout(x, cy, w, share);
        cy = static_cast<uint16_t>(cy + share + spacing);
        remaining = (remaining > share) ? static_cast<uint16_t>(remaining - share) : 0;
        remaining_min = (remaining_min > min_h)
                                ? static_cast<uint16_t>(remaining_min - min_h) : 0;
    };
    std::apply([&](const auto&... c) { (place(c), ...); }, children);
}

// Lay children out left-to-right; the horizontal counterpart of
// layout_vertically, using each child's measured/min width the same way.
template <typename Tuple>
inline void layout_horizontally(const Tuple& children, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h, uint16_t spacing) {
    uint16_t count = 0, total_min = 0;
    std::apply([&](const auto&... c) {
        ((total_min = static_cast<uint16_t>(total_min + c.min_width()),
          ++count), ...);
    }, children);

    const uint16_t gaps = (count > 1) ? static_cast<uint16_t>(spacing * (count - 1)) : 0;
    const uint16_t avail = (w > gaps) ? static_cast<uint16_t>(w - gaps) : 0;

    uint16_t cx = x;
    uint16_t remaining = avail;
    uint16_t remaining_min = total_min;
    auto place = [&](const auto& c) {
        uint16_t desired = c.measured_width();
        uint16_t min_w = c.min_width();
        uint16_t min_rest = (remaining_min > min_w)
                                ? static_cast<uint16_t>(remaining_min - min_w) : 0;
        uint16_t max_for = (remaining > min_rest)
                                ? static_cast<uint16_t>(remaining - min_rest) : 0;
        uint16_t share = (desired <= max_for) ? desired : max_for;
        if (share < min_w) share = min_w;
        if (share > remaining) share = remaining;
        c.layout(cx, y, share, h);
        cx = static_cast<uint16_t>(cx + share + spacing);
        remaining = (remaining > share) ? static_cast<uint16_t>(remaining - share) : 0;
        remaining_min = (remaining_min > min_w)
                                ? static_cast<uint16_t>(remaining_min - min_w) : 0;
    };
    std::apply([&](const auto&... c) { (place(c), ...); }, children);
}

inline void draw_rect(const Display& d, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (w < 2 || h < 2) return;

    d.print(x, y, glyph_top_left);
    for (uint16_t i = 1; i < w - 1; ++i)
        d.print(static_cast<uint16_t>(x + i), y, glyph_horizontal);
    d.print(static_cast<uint16_t>(x + w - 1), y, glyph_top_right);

    for (uint16_t j = 1; j < h - 1; ++j) {
        d.print(x, static_cast<uint16_t>(y + j), glyph_vertical);
        d.print(static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + j), glyph_vertical);
    }

    d.print(x, static_cast<uint16_t>(y + h - 1), glyph_bottom_left);
    for (uint16_t i = 1; i < w - 1; ++i)
        d.print(static_cast<uint16_t>(x + i), static_cast<uint16_t>(y + h - 1), glyph_horizontal);
    d.print(static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + h - 1), glyph_bottom_right);
}

class Widget {
public:
    virtual ~Widget() = default;
    virtual void measure(uint16_t avail_w, uint16_t avail_h) const = 0;
    virtual void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const = 0;
    virtual void render(const Display& d) const = 0;
    virtual void tick() const {}

    uint16_t x() const { return x_; }
    uint16_t y() const { return y_; }
    uint16_t width() const { return w_; }
    uint16_t height() const { return h_; }
    uint16_t measured_width() const { return mw_; }
    uint16_t measured_height() const { return mh_; }
    // Smallest box the widget can use while still showing something: the
    // lower bound the greedy layout respects for later children.
    uint16_t min_width() const { return min_w_; }
    uint16_t min_height() const { return min_h_; }

protected:
    mutable uint16_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    mutable uint16_t mw_ = 0, mh_ = 0;
    mutable uint16_t min_w_ = 0, min_h_ = 0;
};

// A text label with optional color (EBO: zero bytes when NoColor) and border.
// Decoration arguments come first, then the string data:
//   Text{"str"}                                          — no decoration
//   Text{TextConfig{.border=true}, "str"}                — border
//   Text{WithColor{...}, "str"}                          — color
//   Text{WithColor{...}, TextConfig{.border=true}, "str"} — color + border
template <typename ColorPolicy = NoColor>
class Text : public Widget, private ColorPolicy {
public:
    Text() : str_(""), border_(false) {}

    explicit Text(const char* str) : str_(str), border_(false) {}

    Text(TextConfig cfg, const char* str)
        : str_(str), border_(cfg.border) {}

    // WithColor constructors are SFINAE-gated so the implicit CTAD guide
    // cannot deduce ColorPolicy from a non-color first argument (e.g. a Row
    // child). Only the explicit deduction guides below select WithColor.
    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Text(WithColor color, const char* str)
        : ColorPolicy(color), str_(str), border_(false) {}

    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Text(WithColor color, TextConfig cfg, const char* str)
        : ColorPolicy(color), str_(str), border_(cfg.border) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        const uint16_t border = border_ ? 2 : 0;
        const uint16_t content = static_cast<uint16_t>(strlen(str_));
        uint32_t full_w = static_cast<uint32_t>(content) + border;
        uint32_t full_h = static_cast<uint32_t>(1) + border;
        mw_ = (full_w > avail_w) ? avail_w : static_cast<uint16_t>(full_w);
        mh_ = (full_h > avail_h) ? avail_h : static_cast<uint16_t>(full_h);
        // Minimum: at least one visible content cell (one char wide, one line
        // tall), plus the border. Empty text asks for nothing (or just the
        // border) so it works as a spacer.
        min_w_ = (content > 0) ? static_cast<uint16_t>(1 + border) : border;
        min_h_ = (content > 0) ? static_cast<uint16_t>(1 + border) : border;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
    }

    void render(const Display& d) const override {
        // A text needs at least one content row (one line, or three with a
        // border) to show anything. When the greedy layout starves a child to
        // zero height, skip it instead of drawing past its box.
        const uint16_t content_h = border_
                                        ? (h_ > 2 ? static_cast<uint16_t>(h_ - 2) : 0)
                                        : h_;
        if (content_h == 0) {
            painted_ = 0;
            return;
        }
        uint16_t vx = x_, vy = y_, vw = w_;
        if (border_) {
            draw_rect(d, x_, y_, w_, h_);
            vx = static_cast<uint16_t>(x_ + 1);
            vy = static_cast<uint16_t>(y_ + 1);
            vw = (w_ > 2) ? static_cast<uint16_t>(w_ - 2) : 0;
        }
        if (vw == 0) {
            painted_ = 0;
            return;
        }
        char sp[2] = {' ', 0};
        for (uint16_t i = 0; i < painted_; ++i)
            d.print(static_cast<uint16_t>(vx + i), vy, sp);

        if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
            d.setColors(static_cast<const ColorPolicy&>(*this).value);
        }
        uint16_t full = static_cast<uint16_t>(strlen(str_));
        uint16_t drawn = 0;
        if (full > vw && static_cast<uint32_t>(scroll_) + vw > full) {
            // The visible window runs past the end of the string: print the
            // tail, a three-space gap, then continue from the beginning.
            const uint16_t cycle = static_cast<uint16_t>(full + scroll_gap);
            char buf[2] = {0, 0};
            for (uint16_t i = 0; i < vw; ++i) {
                uint16_t p = static_cast<uint16_t>(
                    (static_cast<uint32_t>(scroll_) + i) % cycle);
                buf[0] = (p < full) ? str_[p] : ' ';
                d.print(static_cast<uint16_t>(vx + i), vy, buf);
            }
            drawn = vw;
        } else if (scroll_ < full) {
            const char* s = str_ + scroll_;
            uint16_t n = static_cast<uint16_t>(full - scroll_);
            bool elide = n > vw;
            uint16_t show = elide ? static_cast<uint16_t>(vw - 1) : n;
            char buf[2] = {0, 0};
            for (uint16_t i = 0; i < show; ++i) {
                buf[0] = s[i];
                d.print(static_cast<uint16_t>(vx + i), vy, buf);
            }
            if (elide)
                d.print(static_cast<uint16_t>(vx + vw - 1), vy, glyph_ellipsis);
            drawn = static_cast<uint16_t>(show + (elide ? 1 : 0));
        }
        painted_ = drawn;
        if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
            d.setColors(Colors{});
        }
    }

    void setScroll(uint16_t offset) const { scroll_ = offset; }
    uint16_t scroll() const { return scroll_; }

    void tick() const override {
        const uint16_t vw = border_
                                ? (w_ > 2 ? static_cast<uint16_t>(w_ - 2) : 0)
                                : w_;
        uint16_t full = static_cast<uint16_t>(strlen(str_));
        if (full <= vw) {
            scroll_ = 0;
            return;
        }
        uint16_t cycle = static_cast<uint16_t>(full + scroll_gap);
        scroll_ = (static_cast<uint16_t>(scroll_ + 1) >= cycle)
                    ? 0
                    : static_cast<uint16_t>(scroll_ + 1);
    }

private:
    static constexpr uint16_t scroll_gap = 3; // spaces between end and start
    const char* str_;
    bool border_;
    mutable uint16_t scroll_ = 0;
    mutable uint16_t painted_ = 0;
};

// CTAD: pick NoColor (zero-size base) unless a WithColor decoration is the
// first argument.
Text() -> Text<NoColor>;
Text(const char*) -> Text<NoColor>;
Text(TextConfig, const char*) -> Text<NoColor>;
Text(WithColor, const char*) -> Text<WithColor>;
Text(WithColor, TextConfig, const char*) -> Text<WithColor>;

// Lay children top-to-bottom. ColorPolicy is an EBO base: NoColor adds zero
// bytes, WithColor stores a Colors value. Decoration arguments come first:
//   Column{child1, child2}                                     — no decoration
//   Column{LayoutConfig{.border=true}, child1, child2}         — layout
//   Column{WithColor{...}, child1, child2}                     — color
//   Column{WithColor{...}, LayoutConfig{.border=true}, child1, child2} — both
template <typename ColorPolicy = NoColor, typename... Children>
class Column : public Widget, private ColorPolicy {
public:
    explicit Column(const Children&... children)
        : config_{}, children_(children...) {}

    Column(LayoutConfig config, const Children&... children)
        : config_(config), children_(children...) {}

    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Column(WithColor color, const Children&... children)
        : ColorPolicy(color), config_{}, children_(children...) {}

    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Column(WithColor color, LayoutConfig config, const Children&... children)
        : ColorPolicy(color), config_(config), children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        const uint16_t iw = config_.border
                                ? (avail_w > 2 ? static_cast<uint16_t>(avail_w - 2) : 0)
                                : avail_w;
        const uint16_t ih = config_.border
                                ? (avail_h > 2 ? static_cast<uint16_t>(avail_h - 2) : 0)
                                : avail_h;
        uint16_t max_w = 0, total_h = 0, count = 0;
        uint16_t min_max_w = 0, min_total_h = 0;
        std::apply([&](const auto&... c) {
            ((c.measure(iw, ih),
              max_w = max_u16(max_w, c.measured_width()),
              total_h = static_cast<uint16_t>(total_h + c.measured_height()),
              min_max_w = max_u16(min_max_w, c.min_width()),
              min_total_h = static_cast<uint16_t>(min_total_h + c.min_height()),
              ++count), ...);
        }, children_);
        if (count > 1) {
            total_h = static_cast<uint16_t>(
                total_h + config_.spacing * (count - 1));
            min_total_h = static_cast<uint16_t>(
                min_total_h + config_.spacing * (count - 1));
        }
        const uint16_t border = config_.border ? 2 : 0;
        uint32_t full_w = static_cast<uint32_t>(max_w) + border;
        uint32_t full_h = static_cast<uint32_t>(total_h) + border;
        mw_ = (full_w > avail_w) ? avail_w : static_cast<uint16_t>(full_w);
        mh_ = (full_h > avail_h) ? avail_h : static_cast<uint16_t>(full_h);
        // Column minimum: widest child's min width, summed children's min
        // height plus spacing, each grown by the border.
        min_w_ = static_cast<uint16_t>(static_cast<uint32_t>(min_max_w) + border);
        min_h_ = static_cast<uint16_t>(static_cast<uint32_t>(min_total_h) + border);
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
        uint16_t ix = x, iy = y, iw = w, ih = h;
        if (config_.border) {
            ix = static_cast<uint16_t>(x + 1);
            iy = static_cast<uint16_t>(y + 1);
            iw = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
            ih = (h > 2) ? static_cast<uint16_t>(h - 2) : 0;
        }
        layout_vertically(children_, ix, iy, iw, ih, config_.spacing);
    }

    void render(const Display& d) const override {
        if (config_.border) {
            if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
                d.setColors(static_cast<const ColorPolicy&>(*this).value);
            }
            draw_rect(d, x_, y_, w_, h_);
            if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
                d.setColors(Colors{});
            }
        }
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

    void tick() const override {
        std::apply([&](const auto&... c) { (c.tick(), ...); }, children_);
    }

private:
    LayoutConfig config_;
    std::tuple<const Children...> children_;
};

// CTAD: WithColor as the first argument selects the colored specialization;
// LayoutConfig (or nothing) keeps the default NoColor base.
template <typename... C> Column(const C&...) -> Column<NoColor, C...>;
template <typename... C> Column(LayoutConfig, const C&...) -> Column<NoColor, C...>;
template <typename... C> Column(WithColor, const C&...) -> Column<WithColor, C...>;
template <typename... C> Column(WithColor, LayoutConfig, const C&...) -> Column<WithColor, C...>;

// Lay children left-to-right. Same EBO color pattern as Column.
template <typename ColorPolicy = NoColor, typename... Children>
class Row : public Widget, private ColorPolicy {
public:
    explicit Row(const Children&... children)
        : config_{}, children_(children...) {}

    Row(LayoutConfig config, const Children&... children)
        : config_(config), children_(children...) {}

    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Row(WithColor color, const Children&... children)
        : ColorPolicy(color), config_{}, children_(children...) {}

    template <typename CP = ColorPolicy,
              std::enable_if_t<std::is_same_v<CP, WithColor>, int> = 0>
    Row(WithColor color, LayoutConfig config, const Children&... children)
        : ColorPolicy(color), config_(config), children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        const uint16_t iw = config_.border
                                ? (avail_w > 2 ? static_cast<uint16_t>(avail_w - 2) : 0)
                                : avail_w;
        const uint16_t ih = config_.border
                                ? (avail_h > 2 ? static_cast<uint16_t>(avail_h - 2) : 0)
                                : avail_h;
        uint16_t total_w = 0, max_h = 0, count = 0;
        uint16_t min_total_w = 0, min_max_h = 0;
        std::apply([&](const auto&... c) {
            ((c.measure(iw, ih),
              total_w = static_cast<uint16_t>(total_w + c.measured_width()),
              max_h = max_u16(max_h, c.measured_height()),
              min_total_w = static_cast<uint16_t>(min_total_w + c.min_width()),
              min_max_h = max_u16(min_max_h, c.min_height()),
              ++count), ...);
        }, children_);
        if (count > 1) {
            total_w = static_cast<uint16_t>(
                total_w + config_.spacing * (count - 1));
            min_total_w = static_cast<uint16_t>(
                min_total_w + config_.spacing * (count - 1));
        }
        const uint16_t border = config_.border ? 2 : 0;
        uint32_t full_w = static_cast<uint32_t>(total_w) + border;
        uint32_t full_h = static_cast<uint32_t>(max_h) + border;
        mw_ = (full_w > avail_w) ? avail_w : static_cast<uint16_t>(full_w);
        mh_ = (full_h > avail_h) ? avail_h : static_cast<uint16_t>(full_h);
        // Row minimum: summed children's min width plus spacing, tallest
        // child's min height, each grown by the border.
        min_w_ = static_cast<uint16_t>(static_cast<uint32_t>(min_total_w) + border);
        min_h_ = static_cast<uint16_t>(static_cast<uint32_t>(min_max_h) + border);
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;

        uint16_t ix = x, iy = y, iw = w, ih = h;
        if (config_.border) {
            ix = static_cast<uint16_t>(x + 1);
            iy = static_cast<uint16_t>(y + 1);
            iw = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
            ih = (h > 2) ? static_cast<uint16_t>(h - 2) : 0;
        }

        layout_horizontally(children_, ix, iy, iw, ih, config_.spacing);
    }

    void render(const Display& d) const override {
        // Text may point at a live buffer whose length changed since layout.
        // Re-measure and re-layout so each child starts where the previous one
        // currently ends (e.g. "dist: " + "10" + " m").
        measure(w_, h_);
        layout(x_, y_, w_, h_);
        if (config_.border) {
            if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
                d.setColors(static_cast<const ColorPolicy&>(*this).value);
            }
            draw_rect(d, x_, y_, w_, h_);
            if constexpr (!std::is_same_v<ColorPolicy, NoColor>) {
                d.setColors(Colors{});
            }
        }
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

    void tick() const override {
        std::apply([&](const auto&... c) { (c.tick(), ...); }, children_);
    }

private:
    LayoutConfig config_;
    std::tuple<const Children...> children_;
};

// CTAD: same pattern as Column — WithColor first selects the colored
// specialization, LayoutConfig (or nothing) keeps the default NoColor base.
template <typename... C> Row(const C&...) -> Row<NoColor, C...>;
template <typename... C> Row(LayoutConfig, const C&...) -> Row<NoColor, C...>;
template <typename... C> Row(WithColor, const C&...) -> Row<WithColor, C...>;
template <typename... C> Row(WithColor, LayoutConfig, const C&...) -> Row<WithColor, C...>;

class Screen : public Widget {
public:
    explicit Screen(const Widget& root) : root_(root) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        root_.measure(avail_w, avail_h);
        mw_ = root_.measured_width();
        mh_ = root_.measured_height();
        min_w_ = root_.min_width();
        min_h_ = root_.min_height();
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        root_.layout(x, y, w, h);
        x_ = x; y_ = y; w_ = w; h_ = h;
    }

    void render(const Display& d) const override {
        root_.render(d);
    }

    void tick() const override {
        root_.tick();
    }

private:
    const Widget& root_;
};

template <uint16_t Width, uint16_t Height>
class SimpleTui {
public:
    static constexpr uint32_t redraw_interval_ms = 500;

    explicit SimpleTui(const Display& display) : display_(display) {
        display_.clear();
    }

    template <typename T>
typename std::enable_if<
        std::is_base_of<Widget, typename std::decay<T>::type>::value &&
        std::is_lvalue_reference<T>::value, void>::type
    setScreen(T&& screen) {
        // Only lvalues are accepted: SimpleTui stores a pointer to the screen,
        // so a temporary would dangle once setScreen returns.
        current_ = &screen;
        started_ = false;
        display_.clear();
        screen.measure(Width, Height);
        screen.layout(0, 0, Width, Height);
        screen.render(display_);
    }

    void loop(uint32_t now_ms) {
        if (current_ == nullptr) return;
        if (!started_) {
            started_ = true;
            last_redraw_ms_ = now_ms;
            return;
        }
        if (static_cast<uint32_t>(now_ms - last_redraw_ms_) < redraw_interval_ms)
            return;
        last_redraw_ms_ = now_ms;
        current_->tick();
        current_->render(display_);
    }

    void drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const {
        draw_rect(display_, x, y, width, height);
    }

    const Display& display() const { return display_; }

private:
    const Display& display_;
    const Widget* current_ = nullptr;
    uint32_t last_redraw_ms_ = 0;
    bool started_ = false;
};

} // namespace stui
