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

inline constexpr Color color_white{255, 255, 255};
inline constexpr Color color_green{0, 255, 0};

class Display {
public:
    virtual ~Display() = default;
    virtual void clear() const = 0;
    // Monochrome backends can ignore this; the default keeps them compatible.
    virtual void setColor(Color) const {}
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

// Shrink one child's measured size proportionally when all children together
// don't fit the space the parent assigned to them.
inline uint16_t fit_dimension(uint16_t desired, uint16_t total, uint16_t avail) {
    if (total == 0 || desired == 0)
        return 0;
    if (total <= avail)
        return desired;
    return static_cast<uint16_t>(
        static_cast<uint32_t>(desired) * avail / total);
}

// Lay children out top-to-bottom in the given box. Heights are allocated
// sequentially from the remaining budget, and a non-empty child always gets
// at least one row, so small content children are never rounded to zero and
// overlapped by a following fill child.
template <typename Tuple>
inline void layout_vertically(const Tuple& children, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h) {
    uint16_t total_h = 0;
    std::apply([&](const auto&... c) {
        ((total_h = static_cast<uint16_t>(total_h + c.measured_height())), ...);
    }, children);

    uint16_t cy = y;
    uint16_t remaining = h;
    uint16_t remaining_desired = total_h;
    auto place = [&](const auto& c) {
        uint16_t desired = c.measured_height();
        uint16_t share = fit_dimension(desired, remaining_desired, remaining);
        if (desired > 0 && share == 0 && remaining > 0)
            share = 1;
        if (share > remaining)
            share = remaining;
        c.layout(x, cy, w, share);
        cy = static_cast<uint16_t>(cy + share);
        remaining = static_cast<uint16_t>(remaining - share);
        remaining_desired = (remaining_desired > desired)
                                ? static_cast<uint16_t>(remaining_desired - desired)
                                : 0;
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

protected:
    mutable uint16_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    mutable uint16_t mw_ = 0, mh_ = 0;
};

class Text : public Widget {
public:
    explicit Text(const char* str, Color color = color_white)
        : str_(str), color_(color) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        (void) avail_w;
        (void) avail_h;
        mw_ = static_cast<uint16_t>(strlen(str_));
        mh_ = 1;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
    }

    void render(const Display& d) const override {
        if (w_ == 0) {
            painted_ = 0;
            return;
        }
        char sp[2] = {' ', 0};
        for (uint16_t i = 0; i < painted_; ++i)
            d.print(static_cast<uint16_t>(x_ + i), y_, sp);

        d.setColor(color_);
        uint16_t full = static_cast<uint16_t>(strlen(str_));
        uint16_t drawn = 0;
        if (full > w_ && static_cast<uint32_t>(scroll_) + w_ > full) {
            // The visible window runs past the end of the string: print the
            // tail, a three-space gap, then continue from the beginning.
            const uint16_t cycle = static_cast<uint16_t>(full + scroll_gap);
            char buf[2] = {0, 0};
            for (uint16_t i = 0; i < w_; ++i) {
                uint16_t p = static_cast<uint16_t>(
                    (static_cast<uint32_t>(scroll_) + i) % cycle);
                buf[0] = (p < full) ? str_[p] : ' ';
                d.print(static_cast<uint16_t>(x_ + i), y_, buf);
            }
            drawn = w_;
        } else if (scroll_ < full) {
            const char* s = str_ + scroll_;
            uint16_t n = static_cast<uint16_t>(full - scroll_);
            bool elide = n > w_;
            uint16_t show = elide ? static_cast<uint16_t>(w_ - 1) : n;
            char buf[2] = {0, 0};
            for (uint16_t i = 0; i < show; ++i) {
                buf[0] = s[i];
                d.print(static_cast<uint16_t>(x_ + i), y_, buf);
            }
            if (elide)
                d.print(static_cast<uint16_t>(x_ + w_ - 1), y_, glyph_ellipsis);
            drawn = static_cast<uint16_t>(show + (elide ? 1 : 0));
        }
        painted_ = drawn;
        d.setColor(color_white);
    }

    void setScroll(uint16_t offset) const { scroll_ = offset; }
    uint16_t scroll() const { return scroll_; }

    void tick() const override {
        uint16_t full = static_cast<uint16_t>(strlen(str_));
        if (full <= w_) {
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
    Color color_;
    mutable uint16_t scroll_ = 0;
    mutable uint16_t painted_ = 0;
};

template <typename... Children>
class Column : public Widget {
public:
    Column(const Children&... children) : children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        uint16_t max_w = 0, total_h = 0;
        std::apply([&](const auto&... c) {
            ((c.measure(avail_w, avail_h),
              max_w = max_u16(max_w, c.measured_width()),
              total_h = static_cast<uint16_t>(total_h + c.measured_height())), ...);
        }, children_);
        mw_ = max_w;
        mh_ = (total_h > avail_h) ? avail_h : total_h;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
        layout_vertically(children_, x, y, w, h);
    }

    void render(const Display& d) const override {
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

    void tick() const override {
        std::apply([&](const auto&... c) { (c.tick(), ...); }, children_);
    }

private:
    std::tuple<const Children...> children_;
};

template <typename... Children>
class Row : public Widget {
public:
    explicit Row(const Children&... children) : Row(0, children...) {}

    Row(uint16_t spacing, const Children&... children)
        : spacing_(spacing), children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        uint16_t total_w = 0, max_h = 0, count = 0;
        std::apply([&](const auto&... c) {
            ((c.measure(avail_w, avail_h),
              total_w = static_cast<uint16_t>(total_w + c.measured_width()),
              max_h = max_u16(max_h, c.measured_height()),
              ++count), ...);
        }, children_);
        if (count > 1)
            total_w = static_cast<uint16_t>(total_w + spacing_ * (count - 1));
        mw_ = (total_w > avail_w) ? avail_w : total_w;
        mh_ = max_h;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;

        uint16_t total_w = 0, count = 0;
        std::apply([&](const auto&... c) {
            ((total_w = static_cast<uint16_t>(total_w + c.measured_width()),
              ++count), ...);
        }, children_);
        const uint16_t gaps = (count > 1) ? static_cast<uint16_t>(spacing_ * (count - 1)) : 0;
        const uint16_t avail = (w > gaps) ? static_cast<uint16_t>(w - gaps) : 0;

        uint16_t cx = x;
        std::apply([&](const auto&... c) {
            ((c.layout(cx, y, fit_dimension(c.measured_width(), total_w, avail), h),
              cx = static_cast<uint16_t>(cx + fit_dimension(c.measured_width(), total_w, avail) + spacing_)), ...);
        }, children_);
    }

    void render(const Display& d) const override {
        // Text may point at a live buffer whose length changed since layout.
        // Re-measure and re-layout so each child starts where the previous one
        // currently ends (e.g. "dist: " + "10" + " m").
        measure(w_, h_);
        layout(x_, y_, w_, h_);
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

    void tick() const override {
        std::apply([&](const auto&... c) { (c.tick(), ...); }, children_);
    }

private:
    uint16_t spacing_ = 0;
    std::tuple<const Children...> children_;
};

template <typename... Children>
class Panel : public Widget {
public:
    Panel(const Children&... children) : children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        mw_ = avail_w;
        mh_ = avail_h;
        uint16_t iw = (avail_w > 2) ? static_cast<uint16_t>(avail_w - 2) : 0;
        uint16_t ih = (avail_h > 2) ? static_cast<uint16_t>(avail_h - 2) : 0;
        std::apply([&](const auto&... c) { (c.measure(iw, ih), ...); }, children_);
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
        uint16_t ix = static_cast<uint16_t>(x + 1);
        uint16_t iy = static_cast<uint16_t>(y + 1);
        uint16_t iw = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
        uint16_t ih = (h > 2) ? static_cast<uint16_t>(h - 2) : 0;
        layout_vertically(children_, ix, iy, iw, ih);
    }

    void render(const Display& d) const override {
        draw_rect(d, x_, y_, w_, h_);
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

    void tick() const override {
        std::apply([&](const auto&... c) { (c.tick(), ...); }, children_);
    }

private:
    std::tuple<const Children...> children_;
};

class Screen : public Widget {
public:
    explicit Screen(const Widget& root) : root_(root) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        root_.measure(avail_w, avail_h);
        mw_ = root_.measured_width();
        mh_ = root_.measured_height();
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
