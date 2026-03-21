#pragma once
#ifndef TINT_SHAPER_HPP
#define TINT_SHAPER_HPP

#include <memory>
#include <string_view>
#include "ShapedText.hpp"

using hb_buffer_t = struct hb_buffer_t;
using hb_feature_t = struct hb_feature_t;

namespace tint {
    class Font;

    /// @brief Class that controls shaping and measurement of text using HarfBuzz.
    class Shaper {
    public:
        Shaper();
        ~Shaper();

        Shaper(Shaper const&) = delete;
        Shaper& operator=(Shaper const&) = delete;
        Shaper(Shaper&&) noexcept;
        Shaper& operator=(Shaper&&) noexcept;

        /// @brief Shapes the given string of text using provided font and config.
        /// @param font Font to use for fetching glyph information.
        /// @param text String of text to shape.
        /// @param opts Shaping options, such as text alignment and direction, as well as kerning and ligatures.
        /// @return ShapedText containing the list of shaped glyphs and their positions (without atlas information).
        [[nodiscard]] ShapedText shape(
            Font const& font,
            std::string_view text,
            ShaperOptions const& opts = {}
        ) const;

        /// @brief Shapes the given string of multiline text using provided font and config.
        /// @param font Font to use for fetching glyph information.
        /// @param text String of text to shape. Can contain newline characters to indicate line breaks.
        /// @param opts Shaping options, such as text alignment and direction, as well as kerning and ligatures.
        /// @return ShapedParagraph containing the list of shaped lines, each with their own list of shaped glyphs and positions (without atlas information).
        [[nodiscard]] ShapedParagraph shapeMultiline(
            Font const& font,
            std::string_view text,
            ShaperOptions const& opts = {}
        ) const;

        /// @brief Measures the dimensions of the given string of text when shaped with provided font and config.
        /// @param font Font to use for fetching glyph information.
        /// @param text String of text to measure.
        /// @param opts Shaping options, such as text alignment and direction, as well as kerning and ligatures.
        /// @return TextMetrics containing the width, height, ascender, descender and line height of the shaped text.
        [[nodiscard]] TextMetrics measure(
            Font const& font,
            std::string_view text,
            ShaperOptions const& opts = {}
        ) const;

        /// @brief Measures the dimensions of the given string of multiline text when shaped with provided font and config.
        /// @param font Font to use for fetching glyph information.
        /// @param text String of text to measure. Can contain newline characters to indicate line breaks.
        /// @param opts Shaping options, such as text alignment and direction, as well as kerning and ligatures.
        /// @return TextMetrics containing the width, height, ascender, descender and line height of the shaped multiline text.
        [[nodiscard]] TextMetrics measureMultiline(
            Font const& font,
            std::string_view text,
            ShaperOptions const& opts = {}
        ) const;

    private:
        [[nodiscard]] std::pair<ShapedText, float> shapeSegment(
            Font const& font,
            std::string_view segment,
            ShaperOptions const& opts
        ) const;

        static void applyFeatures(
            Font const& font,
            ShaperOptions const& opts,
            hb_feature_t* features,
            size_t& count
        );

        hb_buffer_t* m_buffer = nullptr;
    };
} // namespace tint

#endif // TINT_SHAPER_HPP