#include <Tint/Font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>

#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

static std::string pathToString(std::filesystem::path const& path) {
    int size = WideCharToMultiByte(
        CP_UTF8, 0,
        path.c_str(), static_cast<int>(path.native().size()),
        nullptr, 0,
        nullptr, nullptr
    );
    std::string str(size, 0);
    WideCharToMultiByte(
        CP_UTF8, 0,
        path.c_str(), static_cast<int>(path.native().size()),
        str.data(), size,
        nullptr,nullptr
    );
    return str;
}
#endif

namespace tint {
    FontLibrary::FontLibrary() {
        FT_Init_FreeType(&m_library);
    }

    FontLibrary::~FontLibrary() {
        if (m_library) FT_Done_FreeType(m_library);
    }

    FontLibrary::FontLibrary(FontLibrary&& other) noexcept
        : m_library(std::exchange(other.m_library, nullptr)) {}

    FontLibrary& FontLibrary::operator=(FontLibrary&& other) noexcept {
        if (this != &other) {
            if (m_library) FT_Done_FreeType(m_library);
            m_library = std::exchange(other.m_library, nullptr);
        }
        return *this;
    }

    Font::~Font() {
        if (m_hbFont) hb_font_destroy(m_hbFont);
        if (m_face) FT_Done_Face(m_face);
    }

    Font::Font(Font&& other) noexcept
        : m_fontData(std::move(other.m_fontData)),
          m_face(std::exchange(other.m_face, nullptr)),
          m_hbFont(std::exchange(other.m_hbFont, nullptr)),
          m_config(other.m_config),
          m_lineHeight(other.m_lineHeight),
          m_ascender(other.m_ascender),
          m_descender(other.m_descender) {}

    Font& Font::operator=(Font&& other) noexcept {
        if (this != &other) {
            if (m_hbFont) hb_font_destroy(m_hbFont);
            if (m_face) FT_Done_Face(m_face);

            m_fontData = std::move(other.m_fontData);
            m_face = std::exchange(other.m_face, nullptr);
            m_hbFont = std::exchange(other.m_hbFont, nullptr);
            m_config = other.m_config;
            m_lineHeight = other.m_lineHeight;
            m_ascender = other.m_ascender;
            m_descender = other.m_descender;
        }
        return *this;
    }

    GlyphMetrics Font::glyphMetrics(uint32_t glyphIndex) const {
        GlyphMetrics m;
        m.glyphIndex = glyphIndex;

        if (FT_Load_Glyph(m_face, glyphIndex, FT_LOAD_DEFAULT) != 0) {
            return m;
        }

        auto const& g = m_face->glyph->metrics;
        m.width = static_cast<int32_t>(g.width >> 6);
        m.height = static_cast<int32_t>(g.height >> 6);
        m.bearingX = static_cast<int32_t>(g.horiBearingX >> 6);
        m.bearingY = static_cast<int32_t>(g.horiBearingY >> 6);
        m.advanceX = static_cast<int32_t>(g.horiAdvance);
        m.advanceY = static_cast<int32_t>(g.vertAdvance);

        return m;
    }

    std::unique_ptr<Font> Font::fromFace(FT_Face face, FontConfig const& cfg) {
        FT_Set_Char_Size(
            face, 0,
            static_cast<FT_F26Dot6>(cfg.sizePx * 64.f),
            cfg.dpi, cfg.dpi
        );

        hb_font_t* hbFont = hb_ft_font_create_referenced(face);
        if (!hbFont) {
            FT_Done_Face(face);
            return nullptr;
        }

        auto font = std::unique_ptr<Font>(new Font());
        font->m_face = face;
        font->m_hbFont = hbFont;
        font->m_config = cfg;
        font->m_lineHeight = static_cast<float>(face->size->metrics.height)   / 64.f;
        font->m_ascender = static_cast<float>(face->size->metrics.ascender)  / 64.f;
        font->m_descender = static_cast<float>(face->size->metrics.descender) / 64.f;

        return font;
    }

    std::unique_ptr<Font> Font::loadFile(
        FontLibrary const& fontLibrary,
        std::filesystem::path const& path,
        FontConfig const& fontConfig
    ) {
        if (!fontLibrary.isValid()) return nullptr;

        FT_Face face = nullptr;
        #if defined(_WIN32) || defined(_WIN64)
        std::string pathStr = pathToString(path);
        if (FT_New_Face(fontLibrary.library(), pathStr.c_str(), 0, &face) != 0) {
            return nullptr;
        }
        #else
        if (FT_New_Face(fontLibrary.library(), path.c_str(), 0, &face) != 0) {
            return nullptr;
        }
        #endif

        return fromFace(face, fontConfig);
    }

    std::unique_ptr<Font> Font::loadMemory(
        FontLibrary const& fontLibrary,
        std::vector<uint8_t> data,
        FontConfig const& fontConfig
    ) {
        std::span dataSpan(data.data(), data.size());
        auto font = loadMemory(fontLibrary, dataSpan, fontConfig);
        if (font) font->m_fontData = std::move(data);
        return font;
    }

    std::unique_ptr<Font> Font::loadMemory(
        FontLibrary const& fontLibrary,
        std::span<uint8_t const> data,
        FontConfig const& fontConfig
    ) {
        if (!fontLibrary.isValid() || data.empty()) return nullptr;

        FT_Face face = nullptr;
        if (FT_New_Memory_Face(
            fontLibrary.library(),
            data.data(),
            static_cast<FT_Long>(data.size()),
            0,
            &face
        ) != 0) {
            return nullptr;
        }

        return fromFace(face, fontConfig);
    }
} // namespace tint