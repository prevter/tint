# tint

Tiny text shaping/rendering library in C++.

## Features

- Shaping and rendering of text using FreeType and HarfBuzz.
- Multiline text layout and word wrapping.
- Multi-atlas support (automatically allocates new atlas pages when needed).
- Supports atlas generation with various output formats:
    - `Bitmap` (grayscale and RGBA);
    - `SDF` (signed distance field);
    - `MSDF`* (multi-channel signed distance field);
    - `MTSDF`* (multi-channel true signed distance field).

\* Atlas generation for `MSDF`/`MTSDF` is only supported via `tntutil` command-line tool.
This is because the generation is computationally expensive and also will produce artifacts if `msdfgen`
is not configured with Skia (see [this issue](https://github.com/Chlumsky/msdfgen/issues/110) for details).

## Usage

```cpp
#include <Tint/Tint.hpp>

int main() {
    // Create freetype library handle (RAII)
    tint::FontLibrary fl;

    // Load the font file
    auto font = tint::Font::loadFile(fl, "path/to/font.ttf", FontConfig {
        .sizePx = 48.f, // Font size in pixels
        .dpi = 72,      // Font DPI
        
        // Optional features (both are enabled by default, can be omitted here)
        .enableLigatures = true, 
        .enableKerning = true
    });
    if (!font) {
        std::cerr << "Failed to load font!" << std::endl;
        return 1;
    }

    // Create a font atlas group with the loaded font
    tint::AtlasGroup atlasGroup(
        1024, 1024, // Atlas size
        PixelFormat::Gray8, // Pixel format
        AtlasMode::SDF, 4.f // Atlas mode and SDF parameters
    );
    
    // Create shaper
    tint::Shaper shaper;
    
    // Shape the text
    auto shapedText = shaper.shape(*font, "Hello, World!");
    auto quads = atlasGroup.buildQuads(shapedText);
    
    // Render quads with your backend of choice (e.g. OpenGL)
    // GlyphQuad contains:
    // - `pageIndex`: index of the atlas texture
    // - `x0`, `y0`, `x1`, `y1`: glyph quad coordinates in screen space
    // - `u0`, `v0`, `u1`, `v1`: texture coordinates in the atlas

    return 0;
}
```

## Integration

The library can be built using CMake. It will automatically download and build
the required dependencies (FreeType and HarfBuzz) using CPM.

This also means you can easily include `tint` as a dependency in your own CMake project using CPM:

```cmake
CPMAddPackage("gh:prevter/tint@1.0.0")
target_link_libraries(your_target PRIVATE tint::tint)
```

## TNTUtil

`tntutil` is a command-line tool included in this repository, that can be used to pre-generate
font atlases for a given .ttf/.otf font file. Output format is a custom .tnt file, which can be loaded
like this:

```cpp
auto [font, atlasGroup] = tint::loadTntFont("path/to/font.tnt");
```

Type `tntutil` without arguments to see the usage instructions:

```
Tint font file builder (.TNT)

Usage: tntutil [OPTIONS] <INPUT> <OUTPUT>

Arguments:
  INPUT: Input TTF/OTF font file
  OUTPUT: Output .tnt file

Options:
  -m, --mode <value>: Rasterisation mode: bitmap | sdf | msdf | mtsdf  (default: mtsdf)
  -s, --size <value>: Font size in pixels  (default: 32)
  -r, --range <value>: SDF/MSDF pixel distance range  (default: 4.0)
  -d, --dpi <value>: DPI  (default: 72)
  -a, --atlas <value>: Atlas page size in pixels  (default: 1024)
  -c, --charset <value>: Characters to include: ascii | <path-to-file>  (default: ascii)
```

## Third-party libraries
- [FreeType](https://www.freetype.org/) for font loading and rasterization (licensed under the [FreeType License](licenses/freetype.txt))
- [HarfBuzz](https://harfbuzz.github.io/) for text shaping (licensed under the ["Old MIT" License](licenses/harfbuzz.txt))
- [LZ4](https://github.com/lz4/lz4) for TNT file compression (licensed under the [BSD 2-Clause License](licenses/lz4.txt))
- [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) for MSDF/MTSDF atlas generation (licensed under the [MIT License](licenses/msdf-atlas-gen.txt))

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

Portions of this software are copyright © 2026 The FreeType
Project (https://freetype.org).  All rights reserved.