#pragma once
#ifndef TINT_FONT_HPP
#define TINT_FONT_HPP

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "Types.hpp"

using FT_Library = struct FT_LibraryRec_*;
using FT_Face = struct FT_FaceRec_*;
using hb_font_t = struct hb_font_t;

namespace tint {
    /// @brief Configuration for loading font data.
    struct FontConfig {
        float    sizePx          = 16.f;
        uint32_t dpi             = 72;
        bool     enableLigatures = true;
        bool     enableKerning   = true;
    };

    /// @brief RAII wrapper for Freetype library handle.
    class FontLibrary {
    public:
        FontLibrary();
        ~FontLibrary();

        FontLibrary(FontLibrary const&) = delete;
        FontLibrary& operator=(FontLibrary const&) = delete;
        FontLibrary(FontLibrary&&) noexcept;
        FontLibrary& operator=(FontLibrary&&) noexcept;

        [[nodiscard]] FT_Library library() const { return m_library; }
        [[nodiscard]] bool isValid() const { return m_library != nullptr; }

    private:
        FT_Library m_library = nullptr;
    };

    /// @brief Class representing a loaded font, containing the Freetype face and HarfBuzz font handles.
    class Font {
    public:
        /// @brief Load a font from a file path.
        /// @param fontLibrary Font library to use for loading the font data.
        /// @param path Path to the font file (e.g. .ttf, .otf).
        /// @param fontConfig Configuration for loading the font, such as size and DPI. Optional, with default values.
        /// @return Unique pointer to the loaded Font object, or nullptr on failure.
        [[nodiscard]] static std::unique_ptr<Font> loadFile(
            FontLibrary const& fontLibrary,
            std::filesystem::path const& path,
            FontConfig const& fontConfig = {}
        );

        /// @brief Load a font from owned memory buffer (no copy).
        /// @param fontLibrary Font library to use for loading the font data.
        /// @param data Memory buffer containing the font file data. It will be moved into the object.
        /// @param fontConfig Configuration for loading the font, such as size and DPI. Optional, with default values.
        /// @return Unique pointer to the loaded Font object, or nullptr on failure.
        [[nodiscard]] static std::unique_ptr<Font> loadMemory(
            FontLibrary const& fontLibrary,
            std::vector<uint8_t> data,
            FontConfig const& fontConfig = {}
        );

        /// @brief Load a font from non-owned memory buffer.
        /// @param fontLibrary Font library to use for loading the font data.
        /// @param data Memory buffer containing the font file data. It will be copied into the object.
        /// @param fontConfig Configuration for loading the font, such as size and DPI. Optional, with default values.
        /// @return Unique pointer to the loaded Font object, or nullptr on failure.
        [[nodiscard]] static std::unique_ptr<Font> loadMemory(
            FontLibrary const& fontLibrary,
            std::span<uint8_t const> data,
            FontConfig const& fontConfig = {}
        );

        ~Font();
        Font(Font const&) = delete;
        Font& operator=(Font const&) = delete;
        Font(Font&&) noexcept;
        Font& operator=(Font&&) noexcept;

        /// @brief Freetype face handle.
        [[nodiscard]] FT_Face face() const noexcept { return m_face; }
        /// @brief HarfBuzz font handle.
        [[nodiscard]] hb_font_t* harfBuzzFont() const noexcept { return m_hbFont; }
        /// @brief Font configuration.
        [[nodiscard]] FontConfig const& config() const noexcept { return m_config; }

        /// @brief Retrieve the metrics for a specified Unicode glyph.
        /// @param glyphIndex Unicode index of the glyph to retrieve metrics for.
        /// @return GlyphMetrics containing the advance, bearing and size of the glyph.
        [[nodiscard]] GlyphMetrics glyphMetrics(uint32_t glyphIndex) const;
        /// @brief Get the common line height of the font.
        [[nodiscard]] float lineHeight() const noexcept { return m_lineHeight; }
        /// @brief Get the ascender height of the font (distance from baseline to the highest point of glyphs).
        [[nodiscard]] float ascender() const noexcept { return m_ascender; }
        /// @brief Get the descender depth of the font (distance from baseline to the lowest point of glyphs).
        [[nodiscard]] float descender() const noexcept { return m_descender; }

    private:
        Font() = default;

        [[nodiscard]] static std::unique_ptr<Font> fromFace(FT_Face face, FontConfig const& config);

        std::vector<uint8_t> m_fontData;
        FT_Face m_face = nullptr;
        hb_font_t* m_hbFont = nullptr;
        FontConfig m_config{};

        float m_lineHeight = 0.0f;
        float m_ascender = 0.0f;
        float m_descender = 0.0f;
    };
} // namespace tint

#endif // TINT_FONT_HPP