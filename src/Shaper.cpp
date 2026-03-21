#include <Tint/Shaper.hpp>

#include <array>
#include <hb.h>
#include <hb-ft.h>
#include <Tint/Font.hpp>

namespace tint {
    Shaper::Shaper() {
        m_buffer = hb_buffer_create();
    }

    Shaper::~Shaper() {
        if (m_buffer) hb_buffer_destroy(m_buffer);
    }

    Shaper::Shaper(Shaper&& other) noexcept
        : m_buffer(std::exchange(other.m_buffer, nullptr)) {}

    Shaper& Shaper::operator=(Shaper&& other) noexcept {
        if (this != &other) {
            if (m_buffer) hb_buffer_destroy(m_buffer);
            m_buffer = std::exchange(other.m_buffer, nullptr);
        }
        return *this;
    }

    void Shaper::applyFeatures(
        Font const& font,
        ShaperOptions const& opts,
        hb_feature_t* features,
        size_t& count
    ) {
        auto const& cfg = font.config();

        bool kern = opts.kerning == ShaperOptions::Kerning::Default
                        ? cfg.enableKerning
                        : opts.kerning == ShaperOptions::Kerning::Enabled;

        bool liga = opts.ligatures == ShaperOptions::Ligatures::Default
                        ? cfg.enableLigatures
                        : opts.ligatures == ShaperOptions::Ligatures::Enabled;

        features[count++] = {
            HB_TAG('l', 'i', 'g', 'a'), liga ? 1u : 0u,
            HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END
        };
        features[count++] = {
            HB_TAG('c', 'l', 'i', 'g'), liga ? 1u : 0u,
            HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END
        };
        features[count++] = {
            HB_TAG('k', 'e', 'r', 'n'), kern ? 1u : 0u,
            HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END
        };
    }

    std::pair<ShapedText, float> Shaper::shapeSegment(
        Font const& font,
        std::string_view segment,
        ShaperOptions const& opts
    ) const {
        if (!m_buffer || segment.empty())
            return {ShapedText{{}, opts.direction}, 0.f};

        hb_buffer_reset(m_buffer);
        hb_buffer_add_utf8(
            m_buffer, segment.data(),
            static_cast<int>(segment.size()),
            0, -1
        );

        if (opts.direction == Direction::LTR)
            hb_buffer_set_direction(m_buffer, HB_DIRECTION_LTR);
        else if (opts.direction == Direction::RTL)
            hb_buffer_set_direction(m_buffer, HB_DIRECTION_RTL);

        hb_buffer_guess_segment_properties(m_buffer);

        std::array<hb_feature_t, 4> features{};
        size_t featureCount = 0;
        applyFeatures(font, opts, features.data(), featureCount);

        hb_shape(
            font.harfBuzzFont(), m_buffer,
            features.data(), static_cast<unsigned>(featureCount)
        );

        uint32_t glyphCount = 0;
        auto* info = hb_buffer_get_glyph_infos(m_buffer, &glyphCount);
        auto* pos = hb_buffer_get_glyph_positions(m_buffer, &glyphCount);

        std::vector<ShapedGlyph> glyphs(glyphCount);
        float advanceX = 0.f;

        for (uint32_t i = 0; i < glyphCount; ++i) {
            ShapedGlyph& g = glyphs[i];
            g.glyphIndex = info[i].codepoint;
            g.cluster = info[i].cluster;
            g.xAdvance = pos[i].x_advance;
            g.yAdvance = pos[i].y_advance;
            g.xOffset = pos[i].x_offset;
            g.yOffset = pos[i].y_offset;

            if (opts.letterSpacing != 0.f)
                g.xAdvance += static_cast<int32_t>(opts.letterSpacing * 64.f);

            advanceX += static_cast<float>(g.xAdvance) / 64.f;
        }

        Direction resolved = opts.direction;
        if (resolved == Direction::Auto) {
            auto hbDir = hb_buffer_get_direction(m_buffer);
            resolved = hbDir == HB_DIRECTION_RTL ? Direction::RTL : Direction::LTR;
        }

        return {ShapedText(std::move(glyphs), resolved), advanceX};
    }

    ShapedText Shaper::shape(
        Font const& font,
        std::string_view text,
        ShaperOptions const& opts
    ) const {
        return shapeSegment(font, text, opts).first;
    }

    ShapedParagraph Shaper::shapeMultiline(
        Font const& font,
        std::string_view text,
        ShaperOptions const& opts
    ) const {
        ShapedParagraph para;

        float lineH = opts.lineHeight > 0.f ? opts.lineHeight : font.lineHeight();
        float cursorY = font.ascender();

        auto pushLine = [&](std::string_view seg) {
            auto [shaped, width] = shapeSegment(font, seg, opts);
            ShapedLine line;
            line.text = std::move(shaped);
            line.baselineY = cursorY;
            line.width = width;
            para.metrics.width = std::max(para.metrics.width, width);
            para.lines.push_back(std::move(line));
            cursorY += lineH;
        };

        auto wrapAndPush = [&](std::string_view lineText) {
            if (opts.maxWidth <= 0.f || lineText.empty()) {
                pushLine(lineText);
                return;
            }

            size_t lineStart = 0;
            size_t lastSpace = std::string_view::npos;
            float runWidth = 0.f;

            auto [fullShaped, _] = shapeSegment(font, lineText, opts);

            std::vector clusterAdvance(lineText.size() + 1, 0.f);
            for (auto const& g : fullShaped.glyphs()) {
                clusterAdvance[g.cluster] += static_cast<float>(g.xAdvance) / 64.f;
            }

            float accumulated = 0.f;
            size_t wrapCandidate = 0;
            float wrapWidth = 0.f;
            size_t pos = lineStart;

            while (pos <= lineText.size()) {
                bool atSpace = pos < lineText.size() && lineText[pos] == ' ';
                bool atEnd = pos == lineText.size();

                accumulated += clusterAdvance[pos];

                if (atSpace) {
                    wrapCandidate = pos;
                    wrapWidth = accumulated;
                }

                if ((accumulated > opts.maxWidth && wrapCandidate > lineStart) || atEnd) {
                    size_t segEnd = atEnd || wrapCandidate == 0 ? pos : wrapCandidate;
                    pushLine(lineText.substr(lineStart, segEnd - lineStart));
                    lineStart = segEnd + (atEnd ? 0 : 1);
                    wrapCandidate = lineStart;
                    accumulated = 0.f;
                    wrapWidth = 0.f;
                    if (atEnd) break;
                    pos = lineStart;
                    continue;
                }
                ++pos;
            }

            if (lineStart < lineText.size())
                pushLine(lineText.substr(lineStart));
        };

        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            std::string_view segment = nl == std::string_view::npos
                                           ? text.substr(start)
                                           : text.substr(start, nl - start);

            wrapAndPush(segment);

            if (nl == std::string_view::npos) break;
            start = nl + 1;

            if (start == text.size()) {
                pushLine({});
                break;
            }
        }

        para.metrics.lineCount = static_cast<uint32_t>(para.lines.size());
        para.metrics.height = cursorY - font.ascender() + font.lineHeight();
        para.metrics.ascender = font.ascender();
        para.metrics.descender = font.descender();

        return para;
    }

    TextMetrics Shaper::measure(
        Font const& font,
        std::string_view text,
        ShaperOptions const& opts
    ) const {
        auto [shaped, width] = shapeSegment(font, text, opts);

        float lineH = opts.lineHeight > 0.f ? opts.lineHeight : font.lineHeight();

        TextMetrics m;
        m.width = width;
        m.height = lineH;
        m.ascender = font.ascender();
        m.descender = font.descender();
        m.lineCount = 1;
        return m;
    }

    TextMetrics Shaper::measureMultiline(
        Font const& font,
        std::string_view text,
        ShaperOptions const& opts
    ) const {
        return shapeMultiline(font, text, opts).metrics;
    }
}
