#pragma once
#ifndef TINT_ATLAS_HPP
#define TINT_ATLAS_HPP

#include <span>
#include <unordered_map>
#include <vector>
#include "Types.hpp"

namespace tint {
    class Font;

    /// @brief Font atlas that stores pixel data for glyphs and their corresponding regions.
    class Atlas {
    public:
        explicit Atlas(
            uint32_t width = 1024,
            uint32_t height = 1024,
            PixelFormat fmt = PixelFormat::Gray8,
            AtlasMode mode = AtlasMode::Bitmap,
            float sdfRange = 4.f
        );

        ~Atlas() noexcept = default;

        Atlas(Atlas const&) = delete;
        Atlas& operator=(Atlas const&) = delete;
        Atlas(Atlas&&) noexcept = default;
        Atlas& operator=(Atlas&&) noexcept = default;

        /// @brief Ensure the specified glyph is present in the atlas, adding it if necessary.
        /// @param font The font to use for rendering the glyph.
        /// @param glyphIndex Unicode index of the glyph
        /// @return AtlasRegion pointer if glyph is present or was successfully added, nullptr otherwise.
        [[nodiscard]] AtlasRegion* ensureGlyph(Font const& font, uint32_t glyphIndex);

        /// @brief Ensure the specified glyphs are present in the atlas, adding them if necessary.
        /// @param font The font to use for rendering the glyphs.
        /// @param glyphIndices Span of Unicode indices of the glyphs to ensure.
        /// @return true if all glyphs are present or were successfully added, false if any glyph failed to be added.
        [[nodiscard]] bool ensureGlyphs(Font const& font, std::span<uint32_t const> glyphIndices);

        /// @brief Looks up the atlas for a specified Unicode character index.
        /// @param glyphIndex Unicode index of the glyph to find.
        /// @return AtlasRegion pointer if found, nullptr if not found.
        [[nodiscard]] AtlasRegion const* findGlyph(uint32_t glyphIndex) const;

        /// @brief Looks up the atlas for a specified Unicode character index.
        /// @param glyphIndex Unicode index of the glyph to find.
        /// @return AtlasRegion pointer if found, nullptr if not found.
        [[nodiscard]] AtlasRegion* findGlyph(uint32_t glyphIndex);

        /// @brief Pixel buffer for the atlas. See `width()`, `height()` and `format()`.
        [[nodiscard]] std::span<uint8_t const> pixels() const noexcept { return m_pixels; }
        /// @brief Width of the atlas in pixels
        [[nodiscard]] uint32_t width() const noexcept { return m_width; }
        /// @brief Height of the atlas in pixels
        [[nodiscard]] uint32_t height() const noexcept { return m_height; }
        /// @brief Pixel format of the atlas. Possible values: RGB8 (MSDF), RGBA8 (MTSDF/Colored bitmap), Gray8 (SDF/Bitmap).
        [[nodiscard]] PixelFormat format() const noexcept { return m_format; }
        /// @brief Texture format of the atlas. Possible values: Bitmap, SDF, MSDF, MTSDF.
        [[nodiscard]] AtlasMode mode() const noexcept { return m_mode; }
        /// @brief SDF range of the atlas (when applicable).
        [[nodiscard]] float sdfRange() const noexcept { return m_sdfRange; }
        /// @brief Glyph regions currently stored in the atlas, mapped by their Unicode glyph index.
        [[nodiscard]] std::unordered_map<uint32_t, AtlasRegion> const& regions() const noexcept { return m_regions; }

        [[nodiscard]] Rect dirtyRect() const noexcept { return m_dirtyRect; }
        void clearDirty() noexcept { m_dirtyRect = {}; }

        [[nodiscard]] static Atlas fromBaked(
            uint32_t width,
            uint32_t height,
            PixelFormat fmt,
            AtlasMode mode, float sdfRange,
            std::vector<uint8_t> pixels,
            std::unordered_map<uint32_t, AtlasRegion> regions
        );

    private:
        Atlas(
            uint32_t w, uint32_t h,
            PixelFormat fmt,
            AtlasMode mode,
            float sdfRange,
            std::vector<uint8_t> pixels,
            std::unordered_map<uint32_t, AtlasRegion> regions
        );

        struct Shelf {
            uint32_t y = 0;
            uint32_t height = 0;
            uint32_t cursor = 0;
        };

        [[nodiscard]] Shelf* findShelf(uint32_t glyphWidth, uint32_t glyphHeight);
        void blit(
            uint8_t const* src, uint32_t srcWidth, uint32_t srcHeight, uint32_t dstX, uint32_t dstY, uint32_t stride
        );
        void expandDirty(Rect const& r);

        [[nodiscard]] uint32_t glyphPadding() const noexcept;

    private:
        std::vector<uint8_t> m_pixels;
        std::vector<Shelf> m_shelves;
        std::unordered_map<uint32_t, AtlasRegion> m_regions;
        int32_t m_nextShelfY = 0;
        Rect m_dirtyRect{};
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        float m_sdfRange = 4.f;
        PixelFormat m_format = PixelFormat::Gray8;
        AtlasMode m_mode = AtlasMode::Bitmap;
    };
} // namespace tint

#endif // TINT_ATLAS_HPP
