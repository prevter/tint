#include <slic.hpp>
#include <Tint/TntFont.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <hb.h>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

template <>
struct slic::ValueParser<tint::AtlasMode> {
    static std::optional<tint::AtlasMode> parse(std::string_view input) noexcept {
        if (input == "bitmap") return tint::AtlasMode::Bitmap;
        if (input == "sdf") return tint::AtlasMode::SDF;
        if (input == "msdf") return tint::AtlasMode::MSDF;
        if (input == "mtsdf") return tint::AtlasMode::MTSDF;
        return std::nullopt;
    }
};

static std::string_view atlasModeToString(tint::AtlasMode mode) noexcept {
    switch (mode) {
        case tint::AtlasMode::Bitmap: return "Bitmap";
        case tint::AtlasMode::SDF: return "SDF";
        case tint::AtlasMode::MSDF: return "MSDF";
        case tint::AtlasMode::MTSDF: return "MTSDF";
        default: return "Unknown";
    }
}

struct Args {
    std::string_view input;
    std::string_view output;
    std::string_view charset = "ascii";
    float size = 32.f;
    float range = 4.f;
    int dpi = 72;
    int atlas = 1024;
    tint::AtlasMode mode = tint::AtlasMode::MTSDF;

    static constexpr std::string_view Description = "Tint font file builder (.TNT)";
    static constexpr std::tuple Options = {
        slic::Arg{"INPUT", &Args::input, "Input TTF/OTF font file"},
        slic::Arg{"OUTPUT", &Args::output, "Output .tnt file"},
        slic::Option{
            "--mode", "-m", &Args::mode,
            "Rasterisation mode: bitmap | sdf | msdf | mtsdf  (default: mtsdf)"
        },
        slic::Option{
            "--size", "-s", &Args::size,
            "Font size in pixels  (default: 32)"
        },
        slic::Option{
            "--range", "-r", &Args::range,
            "SDF/MSDF pixel distance range  (default: 4.0)"
        },
        slic::Option{
            "--dpi", "-d", &Args::dpi,
            "DPI  (default: 72)"
        },
        slic::Option{
            "--atlas", "-a", &Args::atlas,
            "Atlas page size in pixels  (default: 1024)"
        },
        slic::Option{
            "--charset", "-c", &Args::charset,
            "Characters to include: ascii | <path-to-file>  (default: ascii)"
        },
    };
};

static msdf_atlas::Charset charsetFromFile(std::filesystem::path const& path) {
    std::ifstream ifs(path);
    if (!ifs) throw std::runtime_error("Cannot open charset file: " + path.string());

    msdf_atlas::Charset cs;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        uint32_t cp = 0;
        if (line.rfind("U+", 0) == 0 || line.rfind("u+", 0) == 0)
            cp = static_cast<uint32_t>(std::stoul(line.substr(2), nullptr, 16));
        else if (line.size() == 1)
            cp = static_cast<uint8_t>(line[0]);
        else
            cp = static_cast<uint32_t>(std::stoul(line, nullptr, 0));
        if (cp) cs.add(cp);
    }
    return cs;
}

static tint::TntFontWriteParams bakeBitmapOrSDF(Args const& args, std::vector<uint8_t>&& fontData) {
    tint::FontLibrary lib;
    tint::FontConfig cfg;
    cfg.sizePx = args.size;
    cfg.dpi = static_cast<uint32_t>(args.dpi);

    auto font = tint::Font::loadMemory(lib, fontData, cfg);

    if (!font) throw std::runtime_error("Failed to load font for baking");

    tint::AtlasGroup atlasGroup(
        static_cast<uint32_t>(args.atlas),
        static_cast<uint32_t>(args.atlas),
        tint::PixelFormat::Gray8,
        args.mode,
        args.range
    );

    msdf_atlas::Charset cs = (args.charset == "ascii")
                                 ? msdf_atlas::Charset::ASCII
                                 : charsetFromFile(args.charset);

    FT_Face face = font->face();
    std::vector<uint32_t> glyphIndices;
    for (msdf_atlas::unicode_t cp : cs) {
        FT_UInt idx = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
        if (idx != 0) glyphIndices.push_back(idx);
    }

    for (uint32_t idx : glyphIndices)
        (void) atlasGroup.ensureGlyph(*font, idx);

    tint::TntFontWriteParams params;
    params.mode = args.mode;
    params.pixFmt = tint::PixelFormat::Gray8;
    params.sizePx = args.size;
    params.dpi = static_cast<uint32_t>(args.dpi);
    params.ascender = font->ascender();
    params.descender = font->descender();
    params.lineHeight = font->lineHeight();
    params.sdfRange = args.range;
    params.pageWidth = static_cast<uint32_t>(args.atlas);
    params.pageHeight = static_cast<uint32_t>(args.atlas);
    params.fontData = std::move(fontData);

    for (size_t i = 0; i < atlasGroup.pageCount(); ++i) {
        auto pixels = atlasGroup.page(i).pixels();
        params.pages.push_back(std::vector(pixels.begin(), pixels.end()));
    }

    for (uint32_t idx : glyphIndices) {
        auto const* r = atlasGroup.findGlyph(idx);
        if (r) {
            params.glyphIndices.push_back(idx);
            params.regions.push_back(*r);
        }
    }

    return params;
}

static tint::TntFontWriteParams bakeMSDFFont(Args const& args, std::vector<uint8_t>&& fontData) {
    using namespace msdf_atlas;

    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft) throw std::runtime_error("Failed to initialise msdfgen Freetype");

    msdfgen::FontHandle* msdfFont = msdfgen::loadFontData(
        ft,
        reinterpret_cast<msdfgen::byte const*>(fontData.data()),
        static_cast<int>(fontData.size())
    );

    if (!msdfFont) {
        msdfgen::deinitializeFreetype(ft);
        throw std::runtime_error("msdfgen failed to load font");
    }

    Charset cs = args.charset == "ascii"
                     ? Charset::ASCII
                     : charsetFromFile(args.charset);

    std::vector<GlyphGeometry> glyphs;
    FontGeometry fontGeometry(&glyphs);
    fontGeometry.loadCharset(msdfFont, 1.0, cs);

    constexpr double angleThreshold = 3.0;
    constexpr uint64_t coloringSeed = 0;
    for (GlyphGeometry& g : glyphs)
        g.edgeColoring(&msdfgen::edgeColoringInkTrap, angleThreshold, coloringSeed);

    TightAtlasPacker packer;
    constexpr int kMsdfGlyphSpacingPx = 2;
    packer.setDimensions(static_cast<int>(args.atlas), static_cast<int>(args.atlas));
    packer.setScale(args.size);
    packer.setSpacing(kMsdfGlyphSpacingPx);
    packer.setPixelRange(msdfgen::Range(args.range));
    packer.setMiterLimit(1.0);
    int remaining = packer.pack(glyphs.data(), static_cast<int>(glyphs.size()));
    if (remaining > 0)
        std::cerr << "[warn] " << remaining << " glyph(s) did not fit in the atlas\n";

    int atlasW = static_cast<int>(args.atlas), atlasH = static_cast<int>(args.atlas);
    packer.getDimensions(atlasW, atlasH);

    bool isMTSDF = args.mode == tint::AtlasMode::MTSDF;

    std::vector<uint8_t> pixels;
    int bitmapW = atlasW, bitmapH = atlasH;

    GeneratorAttributes attrs;
    attrs.config.overlapSupport = true;
    attrs.scanlinePass = true;

    auto copyBitmap = [&](auto const& bmp, int ch) {
        bitmapW = bmp.width;
        bitmapH = bmp.height;
        pixels.resize(static_cast<size_t>(bitmapW * bitmapH * ch));
        for (int y = 0; y < bitmapH; ++y) {
            int srcY = bitmapH - 1 - y;
            for (int x = 0; x < bitmapW; ++x) {
                auto px = bmp(x, srcY);
                for (int c = 0; c < ch; ++c) {
                    float v = std::clamp(px[c], 0.f, 1.f);
                    pixels[static_cast<size_t>((y * bitmapW + x) * ch + c)] =
                            static_cast<uint8_t>(v * 255.f + 0.5f);
                }
            }
        }
    };

    if (isMTSDF) {
        using Gen = ImmediateAtlasGenerator<float, 4, mtsdfGenerator, BitmapAtlasStorage<float, 4>>;
        Gen gen(atlasW, atlasH);
        gen.setAttributes(attrs);
        gen.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
        gen.generate(glyphs.data(), static_cast<int>(glyphs.size()));
        copyBitmap(static_cast<msdfgen::BitmapConstRef<float, 4>>(gen.atlasStorage()), 4);
    } else {
        using Gen = ImmediateAtlasGenerator<float, 3, msdfGenerator, BitmapAtlasStorage<float, 3>>;
        Gen gen(atlasW, atlasH);
        gen.setAttributes(attrs);
        gen.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
        gen.generate(glyphs.data(), static_cast<int>(glyphs.size()));
        copyBitmap(static_cast<msdfgen::BitmapConstRef<float, 3>>(gen.atlasStorage()), 3);
    }

    msdfgen::FontMetrics metrics{};
    if (!msdfgen::getFontMetrics(metrics, msdfFont, msdfgen::FONT_SCALING_EM_NORMALIZED)) {
        msdfgen::destroyFont(msdfFont);
        msdfgen::deinitializeFreetype(ft);
        throw std::runtime_error("Failed to query font metrics from msdfgen");
    }

    tint::TntFontWriteParams params;
    params.mode = args.mode;
    params.pixFmt = isMTSDF ? tint::PixelFormat::RGBA8 : tint::PixelFormat::RGB8;
    params.sizePx = args.size;
    params.dpi = static_cast<uint32_t>(args.dpi);
    params.ascender = static_cast<float>(metrics.ascenderY * args.size);
    params.descender = static_cast<float>(metrics.descenderY * args.size);
    params.lineHeight = static_cast<float>(metrics.lineHeight * args.size);
    params.sdfRange = args.range;
    params.pageWidth = static_cast<uint32_t>(bitmapW);
    params.pageHeight = static_cast<uint32_t>(bitmapH);
    params.pages.push_back(std::move(pixels));
    params.fontData = std::move(fontData);

    params.regions.reserve(glyphs.size());
    params.glyphIndices.reserve(glyphs.size());

    for (auto const& g : glyphs) {
        if (g.isWhitespace()) continue;

        int gx, gy, gw, gh;
        g.getBoxRect(gx, gy, gw, gh);
        if (gw <= 0 || gh <= 0) continue;

        double pl, pb, pr, pt;
        g.getQuadPlaneBounds(pl, pb, pr, pt);
        (void) pb;
        (void) pr;

        tint::AtlasRegion r;
        r.pageIndex = 0;
        r.rect.x = gx;
        r.rect.y = bitmapH - gy - gh;
        r.rect.w = static_cast<uint32_t>(gw);
        r.rect.h = static_cast<uint32_t>(gh);
        r.bearingX = static_cast<int32_t>(std::lround(pl * args.size));
        r.bearingY = static_cast<int32_t>(std::lround(pt * args.size));

        params.regions.push_back(r);
        params.glyphIndices.push_back(static_cast<uint32_t>(g.getIndex()));
    }

    msdfgen::destroyFont(msdfFont);
    msdfgen::deinitializeFreetype(ft);

    return params;
}

int main(int argc, char** argv) {
    slic::ArgParser<Args> parser(argc, argv);
    auto result = parser.parse();
    if (!result) {
        result.print();
        parser.printHelp();
        return 1;
    }

    Args const& args = parser.result();
    try {
        std::filesystem::path path(args.input);
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) throw std::runtime_error("Cannot open: " + std::string(args.input));

        auto size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        std::vector<uint8_t> fontData(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(fontData.data()), size)) {
            throw std::runtime_error("Failed to read input file");
        }

        std::cout << "Baking " << path.filename().string()
                << " -> " << args.output << " [" << atlasModeToString(args.mode)
                << ", " << args.size << "px]\n";

        tint::TntFontWriteParams params;
        if (args.mode == tint::AtlasMode::MTSDF || args.mode == tint::AtlasMode::MSDF) {
            params = bakeMSDFFont(args, std::move(fontData));
        } else {
            params = bakeBitmapOrSDF(args, std::move(fontData));
        }

        if (!tint::writeTntFont(std::filesystem::path(args.output), params)) {
            throw std::runtime_error("Failed to write output file");
        }

        std::cout << "Done. Pages: " << params.pages.size() << ", glyphs: " << params.regions.size() << "\n";
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
