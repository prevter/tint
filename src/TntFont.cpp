#include <Tint/TntFont.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>

#include <lz4frame.h>

namespace tint {
    static void writeU8(std::ostream& os, uint8_t v) { os.put(static_cast<char>(v)); }

    static void writeU16(std::ostream& os, uint16_t v) {
        uint8_t bytes[2] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
        os.write(reinterpret_cast<char*>(bytes), 2);
    }

    static void writeU32(std::ostream& os, uint32_t v) {
        uint8_t bytes[4] = {
            static_cast<uint8_t>(v),
            static_cast<uint8_t>(v >> 8),
            static_cast<uint8_t>(v >> 16),
            static_cast<uint8_t>(v >> 24)
        };
        os.write(reinterpret_cast<char*>(bytes), 4);
    }

    static void writeI16(std::ostream& os, int16_t v) {
        uint8_t bytes[2] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
        os.write(reinterpret_cast<char*>(bytes), 2);
    }

    static void writeF32(std::ostream& os, float v) { writeU32(os, std::bit_cast<uint32_t>(v)); }

    static uint8_t readU8(uint8_t const*& p) { return *p++; }

    static uint16_t readU16(uint8_t const*& p) {
        uint16_t v = p[0] | p[1] << 8;
        p += 2;
        return v;
    }

    static uint32_t readU32(uint8_t const*& p) {
        uint32_t v = p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
        p += 4;
        return v;
    }

    static int16_t readI16(uint8_t const*& p) { return static_cast<int16_t>(readU16(p)); }
    static float readF32(uint8_t const*& p) { return std::bit_cast<float>(readU32(p)); }

    static void writeBlock(std::ostream& os, std::span<uint8_t const> data) {
        size_t maxDstSize = LZ4F_compressFrameBound(data.size(), nullptr);
        std::vector<uint8_t> compressed(maxDstSize);

        size_t compressedSize = LZ4F_compressFrame(compressed.data(), maxDstSize, data.data(), data.size(), nullptr);
        if (LZ4F_isError(compressedSize)) {
            throw std::runtime_error("LZ4 compression failed");
        }

        writeU32(os, static_cast<uint32_t>(compressedSize));
        os.write(reinterpret_cast<char*>(compressed.data()), compressedSize);
    }

    static bool readBlock(uint8_t const*& p, uint8_t const* end, std::vector<uint8_t>& out) {
        if (p + 4 > end) return false;

        uint32_t compressedSize = readU32(p);
        if (p + compressedSize > end) return false;

        uint8_t const* src = p;
        p += compressedSize;

        LZ4F_decompressionContext_t dctx;
        if (LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION))) {
            fprintf(stderr, "LZ4F context creation failed\n");
            return false;
        }

        LZ4F_frameInfo_t frameInfo{};
        size_t consumed = compressedSize;
        LZ4F_errorCode_t fi = LZ4F_getFrameInfo(dctx, &frameInfo, src, &consumed);
        if (LZ4F_isError(fi)) {
            fprintf(stderr, "LZ4F_getFrameInfo failed: %s\n", LZ4F_getErrorName(fi));
            LZ4F_freeDecompressionContext(dctx);
            return false;
        }

        size_t contentSize = frameInfo.contentSize ? frameInfo.contentSize : compressedSize * 4;

        out.resize(contentSize);
        size_t dstPos = 0;
        size_t srcPos = consumed;

        while (srcPos < compressedSize) {
            size_t dstSize = out.size() - dstPos;
            size_t srcSize = compressedSize - srcPos;

            if (dstSize == 0) {
                out.resize(out.size() * 2);
                dstSize = out.size() - dstPos;
            }

            size_t result = LZ4F_decompress(
                dctx, out.data() + dstPos, &dstSize,
                src + srcPos, &srcSize, nullptr
            );

            if (LZ4F_isError(result)) {
                fprintf(stderr, "LZ4F_decompress failed: %s\n", LZ4F_getErrorName(result));
                LZ4F_freeDecompressionContext(dctx);
                return false;
            }

            dstPos += dstSize;
            srcPos += srcSize;

            if (result == 0) break;
        }

        LZ4F_freeDecompressionContext(dctx);
        out.resize(dstPos);
        return true;
    }

    BakedFont loadTntFont(FontLibrary const& lib, std::filesystem::path const& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return {};

        auto size = file.tellg();
        if (size <= 0) return {};
        file.seekg(0);

        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            fprintf(stderr, "Failed to read file: %s\n", path.string().c_str());
            return {};
        }

        return loadTntFont(lib, data);
    }

    BakedFont loadTntFont(FontLibrary const& lib, std::span<uint8_t const> data) {
        if (data.size() < 40) return {};

        uint8_t const* p = data.data();
        uint8_t const* end = data.data() + data.size();

        if (std::memcmp(p, TNT_MAGIC.data(), TNT_MAGIC.size()) != 0) {
            return {};
        }

        p += TNT_MAGIC.size();

        uint16_t version = readU16(p);
        if (version != TNT_VERSION) return {};

        auto mode = static_cast<AtlasMode>(readU8(p));
        auto pixFmt = static_cast<PixelFormat>(readU8(p));
        float sizePx = readF32(p);
        uint32_t dpi = readU32(p);
        float ascender = readF32(p);
        float descender = readF32(p);
        float lineHeight = readF32(p);
        float sdfRange = readF32(p);
        uint32_t fontDataSize = readU32(p);
        uint32_t glyphCount = readU32(p);
        uint8_t pageCount = readU8(p);
        uint16_t pageWidth = readU16(p);
        uint16_t pageHeight = readU16(p);

        if (pageCount == 0) return {};
        if (p > end) return {};

        FontConfig cfg {
            .sizePx = sizePx,
            .dpi = dpi
        };

        std::vector<uint8_t> fontBytes;
        if (!readBlock(p, end, fontBytes)) return {};
        if (fontBytes.size() != fontDataSize) return {};
        std::unique_ptr<Font> font = Font::loadMemory(lib, std::move(fontBytes), cfg);

        if (!font) return {};

        std::vector<std::unordered_map<uint32_t, AtlasRegion>> pageRegions(pageCount);
        std::unordered_map<uint32_t, uint32_t> glyphToPage;

        constexpr size_t RECORD_SIZE = 17;
        if (p + RECORD_SIZE * glyphCount > end) return {};

        for (uint32_t i = 0; i < glyphCount; ++i) {
            AtlasRegion r;
            uint32_t glyphIndex = readU32(p);
            r.pageIndex = readU8(p);
            r.rect.x = readI16(p);
            r.rect.y = readI16(p);
            r.rect.w = readU16(p);
            r.rect.h = readU16(p);
            r.bearingX = readI16(p);
            r.bearingY = readI16(p);

            if (r.pageIndex >= pageCount) return {};
            pageRegions[r.pageIndex].emplace(glyphIndex, r);
            glyphToPage.emplace(glyphIndex, r.pageIndex);
        }

        uint32_t bpp = bytesPerPixel(pixFmt);
        uint32_t expectedSize = pageWidth * pageHeight * bpp;

        std::vector<std::unique_ptr<Atlas>> pages;
        pages.reserve(pageCount);

        for (uint32_t i = 0; i < pageCount; ++i) {
            std::vector<uint8_t> pixels;
            if (!readBlock(p, end, pixels)) return {};
            if (pixels.size() != expectedSize) return {};

            pages.push_back(std::make_unique<Atlas>(
                Atlas::fromBaked(
                    pageWidth, pageHeight,
                    pixFmt, mode, sdfRange,
                    std::move(pixels),
                    std::move(pageRegions[i])
                )
            ));
        }

        auto atlasGroup = std::make_unique<AtlasGroup>(
            AtlasGroup::fromBaked(
                pageWidth, pageHeight,
                pixFmt, mode, sdfRange,
                std::move(pages),
                std::move(glyphToPage)
            )
        );

        return BakedFont {std::move(font), std::move(atlasGroup)};
    }

    bool writeTntFont(std::filesystem::path const& path, TntFontWriteParams const& params) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        if (params.glyphIndices.size() != params.regions.size()) return false;

        uint32_t bpp = bytesPerPixel(params.pixFmt);
        uint32_t expectedPageSize = params.pageWidth * params.pageHeight * bpp;

        file.write(TNT_MAGIC.data(), TNT_MAGIC.size());
        writeU16(file, TNT_VERSION);
        writeU8(file, static_cast<uint8_t>(params.mode));
        writeU8(file, static_cast<uint8_t>(params.pixFmt));
        writeF32(file, params.sizePx);
        writeU32(file, params.dpi);
        writeF32(file, params.ascender);
        writeF32(file, params.descender);
        writeF32(file, params.lineHeight);
        writeF32(file, params.sdfRange);
        writeU32(file, static_cast<uint32_t>(params.fontData.size()));
        writeU32(file, static_cast<uint32_t>(params.regions.size()));
        writeU8(file, static_cast<uint8_t>(params.pages.size()));
        writeU16(file, static_cast<uint16_t>(params.pageWidth));
        writeU16(file, static_cast<uint16_t>(params.pageHeight));

        writeBlock(file, params.fontData);

        for (size_t i = 0; i < params.regions.size(); ++i) {
            auto const& r = params.regions[i];
            uint32_t glyphIdx = params.glyphIndices[i];
            writeU32(file, glyphIdx);
            writeU8(file, r.pageIndex);
            writeI16(file, r.rect.x);
            writeI16(file, r.rect.y);
            writeU16(file, r.rect.w);
            writeU16(file, r.rect.h);
            writeI16(file, r.bearingX);
            writeI16(file, r.bearingY);
        }

        for (auto const& pageData : params.pages) {
            if (pageData.size() != expectedPageSize) return false;
            writeBlock(file, pageData);
        }

        return file.good();
    }
}
