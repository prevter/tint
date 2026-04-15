#pragma once
#ifndef TINT_ATLAS_GROUP_HPP
#define TINT_ATLAS_GROUP_HPP

#include <memory>
#include <span>
#include <vector>
#include "Atlas.hpp"
#include "Types.hpp"

namespace tint {
    class ShapedText;
    struct ShapedParagraph;
    class Font;

    /// @brief Container for multiple atlas pages, managing glyph placement and lookup across them.
    class AtlasGroup {
    public:
        using PageDirtyCallback = void (*)(void* userData, size_t pageIndex);

        explicit AtlasGroup(
            uint32_t width = 1024,
            uint32_t height = 1024,
            PixelFormat fmt = PixelFormat::Gray8,
            AtlasMode mode = AtlasMode::Bitmap,
            float sdfRange = 4.f
        );

        ~AtlasGroup() = default;

        AtlasGroup(AtlasGroup const&) = delete;
        AtlasGroup& operator=(AtlasGroup const&) = delete;
        AtlasGroup(AtlasGroup&&) noexcept = default;
        AtlasGroup& operator=(AtlasGroup&&) noexcept = default;

        /// @brief Ensure the specified glyph is present in the atlas group, adding it if necessary.
        /// @param font The font to use for rendering the glyph.
        /// @param glyphIndex Unicode index of the glyph
        /// @return AtlasRegion pointer if glyph is present or was successfully added, nullptr otherwise.
        [[nodiscard]] AtlasRegion const* ensureGlyph(Font const& font, uint32_t glyphIndex);

        /// @brief Ensure the specified glyphs are present in the atlas group, adding them if necessary.
        /// @param font The font to use for rendering the glyphs.
        /// @param glyphIndices Span of Unicode indices of the glyphs to ensure.
        /// @return true if all glyphs are present or were successfully added, false if any glyph failed to be added.
        [[nodiscard]] bool ensureGlyphs(Font const& font, std::span<uint32_t const> glyphIndices);

        /// @brief Looks up the atlas group for a specified Unicode character index.
        /// @param glyphIndex Unicode index of the glyph to find.
        /// @return AtlasRegion pointer if found, nullptr if not found.
        [[nodiscard]] AtlasRegion const* findGlyph(uint32_t glyphIndex) const;

        /// @brief Gets the amount of allocated pages in the atlas group.
        [[nodiscard]] size_t pageCount() const noexcept { return m_pages.size(); }
        /// @brief Get the atlas by index.
        [[nodiscard]] Atlas const& page(size_t index) const { return *m_pages[index]; }
        /// @brief Get the atlas by index.
        [[nodiscard]] Atlas& page(size_t index) { return *m_pages[index]; }

        /// @brief Sets a callback function that will be called whenever an atlas page is modified (e.g. when a new glyph is added).
        /// @param callback Function pointer to the callback. The callback will receive the userData pointer and the index of the page that was modified.
        /// @param userData Optional pointer that will be passed to the callback when it is called. Can be used to provide context or state to the callback.
        void setPageDirtyCallback(PageDirtyCallback callback, void* userData = nullptr) noexcept;

        /// @brief Converts shaped text built by `Shaper` class into a list of quads with texture coordinates for rendering.
        /// @param font Font that will be used to ensure glyphs are available.
        /// @param shaped Output of `Shaper::shape()` method.
        /// @param originX Additional horizontal offset to apply to all quads.
        /// @param originY Additional vertical offset to apply to all quads.
        /// @return List of quads with texture coordinates corresponding to the shaped text.
        [[nodiscard]] std::vector<GlyphQuad> buildQuads(
            Font const& font,
            ShapedText const& shaped,
            float originX = 0.f,
            float originY = 0.f
        );

        /// @brief Converts shaped paragraph built by `Shaper` class into a list of quads with texture coordinates for rendering.
        /// @param font Font that will be used to ensure glyphs are available.
        /// @param paragraph Output of `Shaper::shapeMultiline()` method.
        /// @param originX Additional horizontal offset to apply to all quads.
        /// @param originY Additional vertical offset to apply to all quads.
        /// @param align Alignment to apply to each line of the paragraph (Left, Center, Right).
        /// @return List of quads with texture coordinates corresponding to the shaped paragraph.
        [[nodiscard]] std::vector<GlyphQuad> buildQuads(
            Font const& font,
            ShapedParagraph const& paragraph,
            float originX = 0.f,
            float originY = 0.f,
            TextAlign align = TextAlign::Left
        );

        /// @brief Pixel width of each atlas page in the group.
        [[nodiscard]] uint32_t pageWidth() const noexcept { return m_pageWidth; }
        /// @brief Pixel height of each atlas page in the group.
        [[nodiscard]] uint32_t pageHeight() const noexcept { return m_pageHeight; }
        /// @brief Pixel format of the atlas pages in the group. Possible values: RGB8 (MSDF), RGBA8 (MTSDF/Colored bitmap), Gray8 (SDF/Bitmap).
        [[nodiscard]] PixelFormat format() const noexcept { return m_format; }
        /// @brief Texture format of the atlas pages in the group. Possible values: Bitmap, SDF, MSDF, MTSDF.
        [[nodiscard]] AtlasMode mode() const noexcept { return m_mode; }
        /// @brief SDF range of the atlas pages in the group (when applicable).
        [[nodiscard]] float sdfRange() const noexcept { return m_sdfRange; }

        [[nodiscard]] static AtlasGroup fromBaked(
            uint32_t pageWidth, uint32_t pageHeight,
            PixelFormat fmt, AtlasMode mode, float sdfRange,
            std::vector<std::unique_ptr<Atlas>> pages,
            std::unordered_map<uint32_t, uint32_t> glyphToPage
        );

    private:
        struct DefaultTag {};
        AtlasGroup(DefaultTag) {}
        [[nodiscard]] Atlas& allocatePage();
        void notifyPageDirty(size_t pageIndex) const;

        std::vector<std::unique_ptr<Atlas>> m_pages;
        std::unordered_map<uint32_t, uint32_t> m_glyphToPage;
        PageDirtyCallback m_pageDirtyCallback = nullptr;
        void* m_pageDirtyUserData = nullptr;
        uint32_t m_pageWidth = 0;
        uint32_t m_pageHeight = 0;
        float m_sdfRange = 4.f;
        PixelFormat m_format = PixelFormat::Gray8;
        AtlasMode m_mode = AtlasMode::Bitmap;
    };
} // namespace tint

#endif // TINT_ATLAS_GROUP_HPP