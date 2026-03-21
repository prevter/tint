#pragma once
#ifndef TINT_TNTFONT_HPP
#define TINT_TNTFONT_HPP

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

#include "AtlasGroup.hpp"
#include "Font.hpp"
#include "Types.hpp"

namespace tint {
    constexpr std::array TNT_MAGIC = std::to_array<char>({ 'T', 'N', 'T' });
    constexpr uint16_t TNT_VERSION = 1;

    struct BakedFont {
        std::unique_ptr<Font> font;
        std::unique_ptr<AtlasGroup> group;
    };

    /// @brief Load the .TNT font file from a given path.
    /// @param lib Font library to use for loading the font data.
    /// @param path Path to the .TNT file.
    /// @return BakedFont containing the loaded Font and AtlasGroup. Both pointers will be nullptr on error.
    [[nodiscard]] BakedFont loadTntFont(FontLibrary const& lib, std::filesystem::path const& path);

    /// @brief Load the .TNT font file from a memory buffer.
    /// @param lib Font library to use for loading the font data.
    /// @param data Memory buffer containing the contents of a .TNT file.
    /// @return BakedFont containing the loaded Font and AtlasGroup. Both pointers will be nullptr on error.
    [[nodiscard]] BakedFont loadTntFont(FontLibrary const& lib, std::span<uint8_t const> data);

    struct TntFontWriteParams {
        std::vector<uint8_t> fontData;
        std::vector<AtlasRegion> regions;
        std::vector<uint32_t> glyphIndices;
        std::vector<std::vector<uint8_t>> pages;

        float sizePx = 32.f;
        uint32_t dpi = 72;
        float ascender = 0.f;
        float descender = 0.f;
        float lineHeight = 0.f;
        float sdfRange = 4.f;
        uint32_t pageWidth = 1024;
        uint32_t pageHeight = 1024;

        AtlasMode mode = AtlasMode::MTSDF;
        PixelFormat pixFmt = PixelFormat::RGBA8;
    };

    /// @brief Writes a .TNT file using provided parameters. Prefer using tntutil command-line tool.
    /// @param path Path to output file.
    /// @param params Parameters for writing the TNT file, including font data, atlas pages, glyph regions, and metadata.
    /// @return true if writing was successful, false otherwise.
    [[nodiscard]] bool writeTntFont(std::filesystem::path const& path, TntFontWriteParams const& params);
}

#endif // TINT_TNTFONT_HPP