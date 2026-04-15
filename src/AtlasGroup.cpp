#include <cmath>
#include <Tint/AtlasGroup.hpp>
#include <Tint/Font.hpp>
#include <Tint/ShapedText.hpp>

namespace tint {
    AtlasGroup::AtlasGroup(
        uint32_t width, uint32_t height,
        PixelFormat fmt, AtlasMode mode,
        float sdfRange
    ) : m_pageWidth(width), m_pageHeight(height), m_sdfRange(sdfRange), m_format(fmt), m_mode(mode)
    {
        (void) allocatePage();
    }

    void AtlasGroup::setPageDirtyCallback(PageDirtyCallback callback, void* userData) noexcept {
        m_pageDirtyCallback = callback;
        m_pageDirtyUserData = userData;
    }

    void AtlasGroup::notifyPageDirty(size_t pageIndex) const {
        if (m_pageDirtyCallback) {
            m_pageDirtyCallback(m_pageDirtyUserData, pageIndex);
        }
    }

    AtlasRegion const* AtlasGroup::findGlyph(uint32_t glyphIndex) const {
        auto it = m_glyphToPage.find(glyphIndex);
        if (it == m_glyphToPage.end()) {
            return nullptr;
        }
        return m_pages[it->second]->findGlyph(glyphIndex);
    }

    AtlasRegion const* AtlasGroup::ensureGlyph(Font const& font, uint32_t glyphIndex) {
        auto cacheIt = m_glyphToPage.find(glyphIndex);
        if (cacheIt != m_glyphToPage.end())
            return m_pages[cacheIt->second]->findGlyph(glyphIndex);

        for (uint32_t i = 0; i < m_pages.size(); ++i) {
            if (auto* r = m_pages[i]->ensureGlyph(font, glyphIndex)) {
                r->pageIndex = i;
                m_glyphToPage[glyphIndex] = i;
                notifyPageDirty(i);
                return r;
            }
        }

        // all pages are full, create new page
        uint32_t newIdx = static_cast<uint32_t>(m_pages.size());
        AtlasRegion* r = allocatePage().ensureGlyph(font, glyphIndex);
        if (r) {
            r->pageIndex = newIdx;
            m_glyphToPage[glyphIndex] = newIdx;
            notifyPageDirty(newIdx);
        }

        return r;
    }

    bool AtlasGroup::ensureGlyphs(Font const& font, std::span<uint32_t const> glyphIndices) {
        for (uint32_t glyphIndex : glyphIndices) {
            if (!ensureGlyph(font, glyphIndex)) {
                return false;
            }
        }
        return true;
    }

    std::vector<GlyphQuad> AtlasGroup::buildQuads(
        Font const& font,
        ShapedText const& shaped,
        float originX,
        float originY
    ) {
        std::vector<GlyphQuad> quads;
        quads.reserve(shaped.size());

        float cursorX = originX;
        float cursorY = originY;

        float invW = 1.f / static_cast<float>(m_pageWidth);
        float invH = 1.f / static_cast<float>(m_pageHeight);

        for (auto const& sg : shaped.glyphs()) {
            auto* region = ensureGlyph(font, sg.glyphIndex);
            if (region && region->rect.w > 0 && region->rect.h > 0) {
                float xOff = static_cast<float>(sg.xOffset) / 64.f;
                float yOff = static_cast<float>(sg.yOffset) / 64.f;

                float x0 = cursorX + xOff + region->bearingX;
                float y0 = cursorY - yOff - region->bearingY;
                float x1 = x0 + region->rect.w;
                float y1 = y0 + region->rect.h;

                float u0 = region->rect.x * invW;
                float v0 = region->rect.y * invH;
                float u1 = (region->rect.x + region->rect.w) * invW;
                float v1 = (region->rect.y + region->rect.h) * invH;

                quads.push_back(GlyphQuad{
                    .pageIndex = region->pageIndex,
                    .x0 = x0, .y0 = y0,
                    .x1 = x1, .y1 = y1,
                    .u0 = u0, .v0 = v0,
                    .u1 = u1, .v1 = v1
                });
            }

            cursorX += sg.xAdvance / 64.f;
            cursorY += sg.yAdvance / 64.f;
        }

        return quads;
    }

    std::vector<GlyphQuad> AtlasGroup::buildQuads(
        Font const& font,
        ShapedParagraph const& paragraph,
        float originX, float originY,
        TextAlign align
    ) {
        std::vector<GlyphQuad> quads;
        float blockWidth = paragraph.metrics.width;

        for (ShapedLine const& line : paragraph.lines) {
            float lineOffsetX = 0.f;
            if (align == TextAlign::Center)
                lineOffsetX = std::floor((blockWidth - line.width) * 0.5f);
            else if (align == TextAlign::Right)
                lineOffsetX = blockWidth - line.width;

            float baselineX = originX + lineOffsetX;
            float baselineY = originY + line.baselineY;

            auto lineQuads = buildQuads(font, line.text, baselineX, baselineY);
            quads.insert(
                quads.end(),
                std::make_move_iterator(lineQuads.begin()),
                std::make_move_iterator(lineQuads.end())
            );
        }

        return quads;
    }

    AtlasGroup AtlasGroup::fromBaked(
        uint32_t pageWidth, uint32_t pageHeight,
        PixelFormat fmt, AtlasMode mode, float sdfRange,

        std::vector<std::unique_ptr<Atlas>> pages,
        std::unordered_map<uint32_t, uint32_t> glyphToPage
    ) {
        AtlasGroup g(DefaultTag{});
        g.m_pageWidth = pageWidth;
        g.m_pageHeight = pageHeight;
        g.m_format = fmt;
        g.m_mode = mode;
        g.m_sdfRange = sdfRange;
        g.m_pages = std::move(pages);
        g.m_glyphToPage = std::move(glyphToPage);
        return g;
    }

    Atlas& AtlasGroup::allocatePage() {
        m_pages.push_back(std::make_unique<Atlas>(m_pageWidth, m_pageHeight, m_format, m_mode, m_sdfRange));
        return *m_pages.back();
    }
} // namespace tint