#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace lottie_bitmap {

template <typename Pixel, typename Render>
bool renderFit(void *destination, uint32_t width, uint32_t height, size_t stride,
               uint32_t sourceWidth, uint32_t sourceHeight, bool clear, Render render) {
    if (destination == nullptr || width == 0 || height == 0 ||
        sourceWidth == 0 || sourceHeight == 0 ||
        width > std::numeric_limits<size_t>::max() / sizeof(Pixel) ||
        stride < static_cast<size_t>(width) * sizeof(Pixel) ||
        height > std::numeric_limits<size_t>::max() / stride) {
        return false;
    }
    uint32_t fittedWidth = width;
    uint32_t fittedHeight = height;
    if (static_cast<uint64_t>(width) * sourceHeight >
        static_cast<uint64_t>(height) * sourceWidth) {
        fittedWidth = static_cast<uint32_t>(std::max<uint64_t>(1,
                (static_cast<uint64_t>(height) * sourceWidth + sourceHeight / 2) / sourceHeight));
    } else {
        fittedHeight = static_cast<uint32_t>(std::max<uint64_t>(1,
                (static_cast<uint64_t>(width) * sourceHeight + sourceWidth / 2) / sourceWidth));
    }
    const size_t count = static_cast<size_t>(fittedWidth) * fittedHeight;
    if (fittedWidth == width && fittedHeight == height &&
        stride == static_cast<size_t>(width) * sizeof(Pixel)) {
        return render(static_cast<Pixel *>(destination), width, height, count, clear);
    }
    const uint32_t left = (width - fittedWidth) / 2;
    const uint32_t top = (height - fittedHeight) / 2;
    const size_t rowBytes = static_cast<size_t>(fittedWidth) * sizeof(Pixel);
    std::vector<Pixel> buffer(count);
    auto *bytes = static_cast<uint8_t *>(destination);
    if (!clear) {
        for (uint32_t y = 0; y < fittedHeight; ++y) {
            std::memcpy(buffer.data() + static_cast<size_t>(y) * fittedWidth,
                        bytes + static_cast<size_t>(top + y) * stride + static_cast<size_t>(left) * sizeof(Pixel),
                        rowBytes);
        }
    }
    if (!render(buffer.data(), fittedWidth, fittedHeight, count, clear)) {
        return false;
    }
    if (clear) {
        for (uint32_t y = 0; y < height; ++y) {
            std::memset(bytes + static_cast<size_t>(y) * stride, 0, static_cast<size_t>(width) * sizeof(Pixel));
        }
    }
    for (uint32_t y = 0; y < fittedHeight; ++y) {
        std::memcpy(bytes + static_cast<size_t>(top + y) * stride + static_cast<size_t>(left) * sizeof(Pixel),
                    buffer.data() + static_cast<size_t>(y) * fittedWidth, rowBytes);
    }
    return true;
}

}
