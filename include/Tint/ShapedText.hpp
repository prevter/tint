#pragma once
#ifndef TINT_SHAPED_TEXT_HPP
#define TINT_SHAPED_TEXT_HPP

#include <span>
#include <utility>
#include <vector>
#include "Types.hpp"

namespace tint {
    /// @brief Represents a string of shaped glyphs.
    class ShapedText {
    public:
        ShapedText() = default;
        explicit ShapedText(std::vector<ShapedGlyph> glyphs, Direction dir) noexcept
            : m_glyphs(std::move(glyphs)), m_direction(dir) {}

        /// @brief Get the list of shaped glyphs in the text.
        [[nodiscard]] std::span<ShapedGlyph const> glyphs() const noexcept { return m_glyphs; }
        /// @brief Get the text direction (LTR, RTL, Auto) used for shaping.
        [[nodiscard]] Direction direction() const noexcept { return m_direction; }
        /// @brief Check if the shaped text contains any glyphs.
        [[nodiscard]] bool empty() const noexcept { return m_glyphs.empty(); }
        /// @brief Get the number of shaped glyphs in the text.
        [[nodiscard]] size_t size() const noexcept { return m_glyphs.size(); }

    private:
        std::vector<ShapedGlyph> m_glyphs;
        Direction m_direction = Direction::LTR;
    };

    /// @brief Represents a single line in a paragraph.
    struct ShapedLine {
        ShapedText text;
        float baselineY = 0.f;
        float width = 0.f;
    };

    /// @brief A paragraph containing multiple lines of shaped text.
    struct ShapedParagraph {
        std::vector<ShapedLine> lines;
        TextMetrics metrics;
    };
} // namespace tint

#endif // TINT_SHAPED_TEXT_HPP