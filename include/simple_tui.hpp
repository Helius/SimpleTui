#pragma once

#include <stdint.h>
#include <string.h>
#include <tuple>

namespace stui {

class Display {
public:
    virtual ~Display() = default;
    virtual void print(uint16_t x, uint16_t y, const char* str) const = 0;
};

inline uint16_t max_u16(uint16_t a, uint16_t b) { return a > b ? a : b; }

inline void draw_rect(const Display& d, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (w < 2 || h < 2) return;

    d.print(x, y, "\xE2\x94\x8C");
    for (uint16_t i = 1; i < w - 1; ++i)
        d.print(static_cast<uint16_t>(x + i), y, "\xE2\x94\x80");
    d.print(static_cast<uint16_t>(x + w - 1), y, "\xE2\x94\x90");

    for (uint16_t j = 1; j < h - 1; ++j) {
        d.print(x, static_cast<uint16_t>(y + j), "\xE2\x94\x82");
        d.print(static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + j), "\xE2\x94\x82");
    }

    d.print(x, static_cast<uint16_t>(y + h - 1), "\xE2\x94\x94");
    for (uint16_t i = 1; i < w - 1; ++i)
        d.print(static_cast<uint16_t>(x + i), static_cast<uint16_t>(y + h - 1), "\xE2\x94\x80");
    d.print(static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + h - 1), "\xE2\x94\x98");
}

class Widget {
public:
    virtual ~Widget() = default;
    virtual void measure(uint16_t avail_w, uint16_t avail_h) const = 0;
    virtual void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const = 0;
    virtual void render(const Display& d) const = 0;

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
    explicit Text(const char* str) : str_(str) {}

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
        if (w_ == 0) return;
        uint16_t full = static_cast<uint16_t>(strlen(str_));
        if (scroll_ >= full) return;
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
            d.print(static_cast<uint16_t>(x_ + w_ - 1), y_, "\xE2\x80\xA6");
    }

    void setScroll(uint16_t offset) const { scroll_ = offset; }

private:
    const char* str_;
    mutable uint16_t scroll_ = 0;
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
        mh_ = total_h;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
        uint16_t cy = y;
        std::apply([&](const auto&... c) {
            ((c.layout(x, cy, w, c.measured_height()),
              cy = static_cast<uint16_t>(cy + c.measured_height())), ...);
        }, children_);
    }

    void render(const Display& d) const override {
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

private:
    std::tuple<const Children...> children_;
};

template <typename... Children>
class Row : public Widget {
public:
    Row(const Children&... children) : children_(children...) {}

    void measure(uint16_t avail_w, uint16_t avail_h) const override {
        uint16_t total_w = 0, max_h = 0;
        std::apply([&](const auto&... c) {
            ((c.measure(avail_w, avail_h),
              total_w = static_cast<uint16_t>(total_w + c.measured_width()),
              max_h = max_u16(max_h, c.measured_height())), ...);
        }, children_);
        mw_ = total_w;
        mh_ = max_h;
    }

    void layout(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const override {
        x_ = x; y_ = y; w_ = w; h_ = h;
        uint16_t cx = x;
        std::apply([&](const auto&... c) {
            ((c.layout(cx, y, c.measured_width(), h),
              cx = static_cast<uint16_t>(cx + c.measured_width())), ...);
        }, children_);
    }

    void render(const Display& d) const override {
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
    }

private:
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
        uint16_t iw = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
        uint16_t cy = static_cast<uint16_t>(y + 1);
        std::apply([&](const auto&... c) {
            ((c.layout(ix, cy, iw, c.measured_height()),
              cy = static_cast<uint16_t>(cy + c.measured_height())), ...);
        }, children_);
    }

    void render(const Display& d) const override {
        draw_rect(d, x_, y_, w_, h_);
        std::apply([&](const auto&... c) { (c.render(d), ...); }, children_);
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

private:
    const Widget& root_;
};

template <uint16_t Width, uint16_t Height>
class SimpleTui {
public:
    explicit SimpleTui(const Display& display) : display_(display) {}

    void setScreen(const Widget& screen) const {
        screen.measure(Width, Height);
        screen.layout(0, 0, Width, Height);
        screen.render(display_);
    }

    void loop() const {}

    void drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) const {
        draw_rect(display_, x, y, width, height);
    }

    const Display& display() const { return display_; }

private:
    const Display& display_;
};

} // namespace stui
