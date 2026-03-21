#include <Tint/Atlas.hpp>
#include <Tint/Font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tint {
    Atlas::Atlas(uint32_t width, uint32_t height, PixelFormat fmt, AtlasMode mode, float sdfRange)
        : m_width(width), m_height(height), m_sdfRange(sdfRange), m_format(fmt), m_mode(mode)
    {
        m_pixels.resize(m_width * m_height * bytesPerPixel(m_format), 0);
    }

    AtlasRegion const* Atlas::findGlyph(uint32_t glyphIndex) const {
        auto it = m_regions.find(glyphIndex);
        return it != m_regions.end() ? &it->second : nullptr;
    }

    AtlasRegion* Atlas::findGlyph(uint32_t glyphIndex) {
        auto it = m_regions.find(glyphIndex);
        return it != m_regions.end() ? &it->second : nullptr;
    }

    AtlasRegion* Atlas::ensureGlyph(Font const& font, uint32_t glyphIndex) {
        if (auto* region = findGlyph(glyphIndex)) {
            return region;
        }

        FT_Face face = font.face();

        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_BITMAP) != 0)
            return nullptr;

        // convert to bitmap even when using SDF, because without it letters will contain holes
        // if font has self-overlapping shapes (pretty much all Google Fonts contain them lol)
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
            return nullptr;

        if (m_mode == AtlasMode::SDF) {
            if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF) != 0)
                return nullptr;
        }

        FT_Bitmap const& bmp = face->glyph->bitmap;
        uint32_t glyphWidth = bmp.width;
        uint32_t glyphHeight = bmp.rows;

        if (glyphWidth == 0 || glyphHeight == 0) {
            auto [it, _] = m_regions.emplace(glyphIndex, AtlasRegion{
                .pageIndex = 0,
                .rect = {0, 0, 0, 0},
                .bearingX = face->glyph->bitmap_left,
                .bearingY = face->glyph->bitmap_top
            });
            return &it->second;
        }

        uint32_t pad = glyphPadding();
        uint32_t neededW = glyphWidth + pad * 2;
        uint32_t neededH = glyphHeight + pad * 2;

        Shelf* shelf = findShelf(neededW, neededH);
        if (!shelf) {
            return nullptr;
        }

        int dstX = shelf->cursor + pad;
        int dstY = shelf->y + pad;
        shelf->cursor += neededW;

        uint32_t srcStride = static_cast<uint32_t>(std::abs(bmp.pitch));
        this->blit(bmp.buffer, glyphWidth, glyphHeight, dstX, dstY, srcStride);

        Rect rect{dstX, dstY, glyphWidth, glyphHeight};
        this->expandDirty(rect);

        auto [it, _] = m_regions.emplace(glyphIndex, AtlasRegion{
            .pageIndex = 0,
            .rect = rect,
            .bearingX = face->glyph->bitmap_left,
            .bearingY = face->glyph->bitmap_top
        });
        return &it->second;
    }

    bool Atlas::ensureGlyphs(Font const& font, std::span<uint32_t const> glyphIndices) {
        for (uint32_t glyphIndex : glyphIndices) {
            if (!ensureGlyph(font, glyphIndex)) {
                return false;
            }
        }
        return true;
    }

    Atlas::Atlas(
        uint32_t w, uint32_t h,
        PixelFormat fmt, AtlasMode mode,
        float sdfRange,
        std::vector<uint8_t> pixels,
        std::unordered_map<uint32_t, AtlasRegion> regions
    ) : m_pixels(std::move(pixels)), m_regions(std::move(regions)),
        m_width(w), m_height(h), m_sdfRange(sdfRange),
        m_format(fmt), m_mode(mode) {}

    Atlas Atlas::fromBaked(
        uint32_t width, uint32_t height,
        PixelFormat fmt, AtlasMode mode,
        float sdfRange,
        std::vector<uint8_t> pixels,
        std::unordered_map<uint32_t, AtlasRegion> regions
    ) {
        return Atlas(width, height, fmt, mode, sdfRange, std::move(pixels), std::move(regions));
    }

    Atlas::Shelf* Atlas::findShelf(uint32_t glyphWidth, uint32_t glyphHeight) {
        Shelf* best = nullptr;
        int32_t bestWaste = std::numeric_limits<int32_t>::max();

        for (Shelf& shelf : m_shelves) {
            if (shelf.height >= glyphHeight && shelf.cursor + glyphWidth <= m_width) {
                int32_t waste = shelf.height - glyphHeight;
                if (waste < bestWaste) {
                    bestWaste = waste;
                    best = &shelf;
                }
            }
        }

        if (best) {
            return best;
        }

        // create new
        int32_t shelfH = glyphHeight + 1; // 1 pixel padding between shelves
        if (m_nextShelfY + shelfH > m_height) {
            return nullptr;
        }

        m_shelves.push_back(Shelf{
            .y = static_cast<uint32_t>(m_nextShelfY),
            .height = glyphHeight,
            .cursor = 0
        });
        m_nextShelfY += shelfH;
        return &m_shelves.back();
    }

    void Atlas::blit(
        uint8_t const* src,
        uint32_t srcWidth, uint32_t srcHeight,
        uint32_t dstX, uint32_t dstY,
        uint32_t stride
    ) {
        uint32_t bpp = bytesPerPixel(m_format);
        uint32_t atlasStride = m_width * bpp;

        for (uint32_t y = 0; y < srcHeight; ++y) {
            uint8_t const* srcRow = src + stride * y;
            uint8_t* dstRow = m_pixels.data() + (dstY + y) * atlasStride + dstX * bpp;

            if (m_format == PixelFormat::Gray8) {
                std::memcpy(dstRow, srcRow, srcWidth);
            } else {
                for (uint32_t x = 0; x < srcWidth; ++x) {
                    uint8_t value = srcRow[x];
                    dstRow[x * bpp + 0] = value; // R
                    dstRow[x * bpp + 1] = value; // G
                    dstRow[x * bpp + 2] = value; // B
                    if (bpp == 4) {
                        dstRow[x * bpp + 3] = value; // A
                    }
                }
            }
        }
    }

    void Atlas::expandDirty(Rect const& r) {
        if (m_dirtyRect.w == 0 && m_dirtyRect.h == 0) {
            m_dirtyRect = r;
            return;
        }

        int32_t x0 = std::min(m_dirtyRect.x, r.x);
        int32_t y0 = std::min(m_dirtyRect.y, r.y);
        int32_t x1 = std::max(m_dirtyRect.x + m_dirtyRect.w, r.x + r.w);
        int32_t y1 = std::max(m_dirtyRect.y + m_dirtyRect.h, r.y + r.h);
        m_dirtyRect = {x0, y0, static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
    }

    uint32_t Atlas::glyphPadding() const noexcept {
        if (m_mode == AtlasMode::SDF)
            return static_cast<uint32_t>(std::ceil(m_sdfRange));
        return 1u;
    }
} // namespace tint