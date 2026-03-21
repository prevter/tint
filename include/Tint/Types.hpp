#pragma once
#ifndef TINT_TYPES_HPP
#define TINT_TYPES_HPP

#include <cstdint>

namespace tint {
    /// @brief Text direction for shaping and layout.
    enum class Direction : uint8_t {
        LTR, // Left to Right
        RTL, // Right to Left
        Auto // let HarfBuzz guess from the text
    };

    /// @brief Pixel format for atlas textures.
    enum class PixelFormat : uint8_t {
        RGB8,  // 3 bytes per pixel (MSDF)
        RGBA8, // 4 bytes per pixel (color emoji, etc.)
        Gray8  // 1 byte per pixel (bitmap/SDF)
    };

    enum class AtlasMode : uint8_t {
        Bitmap,
        SDF,
        MSDF,
        MTSDF
    };

    enum class TextAlign : uint8_t {
        Left,
        Center,
        Right
    };

    struct Rect {
        int32_t  x = 0;
        int32_t  y = 0;
        uint32_t w = 0;
        uint32_t h = 0;
    };

    struct GlyphMetrics {
        uint32_t glyphIndex = 0;
        int32_t  width      = 0;
        int32_t  height     = 0;
        int32_t  bearingX   = 0;
        int32_t  bearingY   = 0;
        int32_t  advanceX   = 0;
        int32_t  advanceY   = 0;
    };

    struct ShapedGlyph {
        uint32_t glyphIndex = 0;
        uint32_t cluster    = 0;
        int32_t  xAdvance   = 0;
        int32_t  yAdvance   = 0;
        int32_t  xOffset    = 0;
        int32_t  yOffset    = 0;
    };

    struct AtlasRegion {
        uint32_t pageIndex = 0;
        Rect     rect      = {};
        int32_t  bearingX  = 0;
        int32_t  bearingY  = 0;
    };

    struct GlyphQuad {
        uint32_t pageIndex = 0;

        // screen-space corners
        float x0 = 0.f, y0 = 0.f; // top-left
        float x1 = 0.f, y1 = 0.f; // bottom-right

        // texture coordinates
        float u0 = 0.f, v0 = 0.f; // top-left
        float u1 = 0.f, v1 = 0.f; // bottom-right
    };

    struct TextMetrics {
        float width        = 0.f;
        float height       = 0.f;
        float ascender     = 0.f;
        float descender    = 0.f;
        uint32_t lineCount = 0;
    };

    struct ShaperOptions {
        float lineHeight = 0.f;
        float letterSpacing = 0.f;
        float wordSpacing = 0.f;
        float maxWidth = 0.f;

        Direction direction = Direction::Auto;
        TextAlign align = TextAlign::Left;

        enum class Kerning : uint8_t { Default, Disabled, Enabled } kerning = Kerning::Default;
        enum class Ligatures : uint8_t { Default, Disabled, Enabled } ligatures = Ligatures::Default;
    };

    constexpr uint32_t bytesPerPixel(PixelFormat format) {
        switch (format) {
            case PixelFormat::RGB8: return 3;
            case PixelFormat::RGBA8: return 4;
            case PixelFormat::Gray8: return 1;
        }
        return 1;
    }
} // namespace tint

#endif // TINT_TYPES_HPP