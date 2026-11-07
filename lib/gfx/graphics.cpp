//
// Rectangle helpers and the synthetic font used by headless tests.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>

#include "gfx/graphics.h"
#include "util/text.h"

namespace gfx {
namespace {

// Every character is six tenths of an em wide.
class fixed_font final : public font {
public:
    explicit fixed_font(font_request request) : request_(std::move(request)) {}

    const font_request &request() const override { return request_; }

    int ascent() const override {
        return static_cast<int>(std::lround(request_.size_pixels * 0.8));
    }

    int descent() const override {
        return static_cast<int>(std::lround(request_.size_pixels * 0.2));
    }

    int measure(std::string_view text) const override {
        return static_cast<int>(std::lround(
            util::char_count(text) * request_.size_pixels * 0.6));
    }

private:
    font_request request_;
};

} // namespace

device_rect device_rect::intersect(const device_rect &other) const {
    const int left = std::max(x, other.x);
    const int top = std::max(y, other.y);
    const int right = std::min(x + width, other.x + other.width);
    const int bottom = std::min(y + height, other.y + other.height);
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

std::shared_ptr<const font>
fixed_font_provider::find(const font_request &request) {
    return std::make_shared<fixed_font>(request);
}

} // namespace gfx
