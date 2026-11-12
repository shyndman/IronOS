/*
 * LCD_StripEngine.cpp — Zero-extra-SRAM 16bpp RGB565 strip renderer for ST7735 (160x80).
 *
 * Purpose:
 *   Renders anti-aliased Chaos Telemetry UI frames at full 16-bit RGB565 color depth
 *   while fitting inside a 3,200-byte strip buffer (1/8th of a 25.6 KB full framebuffer).
 *
 * How it works:
 *   1. During guiHandleDraw(), draw calls append compact tagged union commands (BoxCmd,
 *      BitmapCmd, TextCmd) to a fixed 80-slot command list + 320-byte text pool.
 *   2. At refreshColor(), an FNV-1a hash of the command list skips SPI transfers whenever
 *      the frame is identical to the previous frame.
 *   3. Otherwise, the screen is rasterized left-to-right in eight 20x80 column-major
 *      RGB565 strips and streamed directly over SPI.
 *
 * Rounded-rectangle anti-aliasing:
 *   Uses standard signed-distance-field (SDF) circle coverage (matching Skia / LVGL
 *   lv_draw_sw_border):
 *     outerCoverage(r) = clamp(r - hypot(r - (ex + 0.5), r - (ey + 0.5)) + 0.5, 0, 1)
 *     innerCoverage    = outerCoverage(r - 1)
 *     strokeCoverage   = outerCoverage - innerCoverage
 *     fillCoverage     = innerCoverage
 *   Evaluating 8-bit [0..255] coverage directly via a constexpr quarter-circle SDF table
 *   preserves full spine brightness (~96%) along rounded shoulders with zero FPU cost.
 */

#include "LCD.hpp"

#ifdef LCD_160x80

#include "Hs02UiAssets.hpp"
#include <stdint.h>
#include <string.h>

namespace {

constexpr int16_t  kStripCols      = 20;
constexpr uint8_t  kStripCount     = 8;
constexpr uint8_t  kMaxDrawCmds    = 80;
constexpr uint16_t kTextPoolSize   = 320;
constexpr uint8_t  kMaxCornerRad   = 8;
constexpr uint16_t kNoColor        = 0x0001u;
constexpr uint16_t kDefaultBgRgb   = 0x0883u; // #0E1319
constexpr uint32_t kFnvOffsetBasis = 2166136261u;
constexpr uint32_t kFnvPrime       = 16777619u;

enum class CmdType : uint8_t { Box, Bitmap, Text };

struct BoxCmd {
  int16_t  x;
  int16_t  y;
  int16_t  w;
  int16_t  h;
  uint16_t fillRgb;
  uint16_t strokeRgb;
  uint8_t  radius;
};

struct BitmapCmd {
  const uint8_t *data;
  int16_t        x;
  int16_t        y;
  int16_t        clipX0;
  uint16_t       fgRgb;
  uint8_t        w;
  uint8_t        h;
  bool           fadeLeftEdge;
};

struct TextCmd {
  int16_t  x;
  int16_t  y;
  int16_t  clipX0;
  int16_t  clipX1;
  uint16_t fgRgb;
  uint16_t textOffset;
  uint8_t  pxSize;
};

struct DrawCmd {
  CmdType type;
  union {
    BoxCmd    box;
    BitmapCmd bitmap;
    TextCmd   text;
  };
};

DrawCmd  drawCmds[kMaxDrawCmds]  = {};
char     textPool[kTextPoolSize] = {};
uint8_t  cmdCount                = 0;
uint16_t textPoolLen             = 0;
uint16_t backgroundRgb           = kDefaultBgRgb;

uint16_t blendRgb565Alpha8(uint16_t dst, uint16_t src, uint8_t alpha) {
  if (alpha == 0u) {
    return dst;
  }

  if (alpha >= 255u) {
    return src;
  }

  const uint32_t dstR = (dst >> 11) & 0x1Fu;
  const uint32_t dstG = (dst >> 5) & 0x3Fu;
  const uint32_t dstB = dst & 0x1Fu;

  const uint32_t srcR = (src >> 11) & 0x1Fu;
  const uint32_t srcG = (src >> 5) & 0x3Fu;
  const uint32_t srcB = src & 0x1Fu;
  const uint32_t invA = 255u - alpha;

  const uint32_t outR = (dstR * invA + srcR * alpha + 127u) / 255u;
  const uint32_t outG = (dstG * invA + srcG * alpha + 127u) / 255u;
  const uint32_t outB = (dstB * invA + srcB * alpha + 127u) / 255u;

  return static_cast<uint16_t>((outR << 11) | (outG << 5) | outB);
}

uint8_t alpha2BitTo8Bit(uint8_t alpha2bit) {
  static constexpr uint8_t kLut[4] = {0u, 85u, 170u, 255u};
  return kLut[alpha2bit & 0x03u];
}

void plotStripPixel(uint16_t *stripBuf, int16_t stripX0, int16_t x, int16_t y, uint16_t color,
                    uint8_t alpha8) {
  if (alpha8 == 0u || x < stripX0 || x >= stripX0 + kStripCols || y < 0 || y >= LCD_HEIGHT) {
    return;
  }

  uint16_t &dst = stripBuf[(x - stripX0) * LCD_HEIGHT + y];
  dst           = blendRgb565Alpha8(dst, color, alpha8);
}

void rasterRoundedCorner(const BoxCmd &box, uint16_t &dstPixel, int16_t edgeX, int16_t edgeY) {
  const uint8_t outerAlpha = Hs02UI::kSdfCircleCoverage[box.radius][edgeY][edgeX];
  if (outerAlpha == 0u) {
    return;
  }

  if (box.strokeRgb == kNoColor) {
    if (box.fillRgb != kNoColor) {
      dstPixel = blendRgb565Alpha8(dstPixel, box.fillRgb, outerAlpha);
    }
    return;
  }

  const uint8_t inRadius   = (box.radius > 1u) ? static_cast<uint8_t>(box.radius - 1u) : 0u;
  const uint8_t innerAlpha = (inRadius > 0u && edgeX > 0 && edgeY > 0)
                                 ? Hs02UI::kSdfCircleCoverage[inRadius][edgeY - 1][edgeX - 1]
                                 : 0u;

  const uint8_t strokeAlpha =
      (outerAlpha > innerAlpha) ? static_cast<uint8_t>(outerAlpha - innerAlpha) : 0u;

  if (box.fillRgb == kNoColor) {
    dstPixel = blendRgb565Alpha8(dstPixel, box.strokeRgb, strokeAlpha);
    return;
  }

  // Composite inner fill + 1px SDF border against background in a single pass.
  const uint32_t bgWeight     = 255u - outerAlpha;
  const uint32_t fillWeight   = innerAlpha;
  const uint32_t strokeWeight = strokeAlpha;

  const uint32_t bgR = (dstPixel >> 11) & 0x1Fu;
  const uint32_t bgG = (dstPixel >> 5) & 0x3Fu;
  const uint32_t bgB = dstPixel & 0x1Fu;

  const uint32_t flR = (box.fillRgb >> 11) & 0x1Fu;
  const uint32_t flG = (box.fillRgb >> 5) & 0x3Fu;
  const uint32_t flB = box.fillRgb & 0x1Fu;

  const uint32_t stR = (box.strokeRgb >> 11) & 0x1Fu;
  const uint32_t stG = (box.strokeRgb >> 5) & 0x3Fu;
  const uint32_t stB = box.strokeRgb & 0x1Fu;

  const uint32_t outR = (bgR * bgWeight + flR * fillWeight + stR * strokeWeight + 127u) / 255u;
  const uint32_t outG = (bgG * bgWeight + flG * fillWeight + stG * strokeWeight + 127u) / 255u;
  const uint32_t outB = (bgB * bgWeight + flB * fillWeight + stB * strokeWeight + 127u) / 255u;

  dstPixel = static_cast<uint16_t>((outR << 11) | (outG << 5) | outB);
}

void rasterBoxCmd(const BoxCmd &box, int16_t stripX0, uint16_t *stripBuf) {
  int16_t xStart = (box.x > stripX0) ? box.x : stripX0;
  int16_t xEnd =
      ((box.x + box.w) < (stripX0 + kStripCols)) ? (box.x + box.w) : (stripX0 + kStripCols);

  if (xStart < 0) {
    xStart = 0;
  }

  if (xEnd > LCD_WIDTH) {
    xEnd = LCD_WIDTH;
  }

  if (xStart >= xEnd) {
    return;
  }

  for (int16_t y = box.y; y < box.y + box.h; ++y) {
    if (y < 0 || y >= LCD_HEIGHT) {
      continue;
    }

    const int16_t distTop    = y - box.y;
    const int16_t distBottom = box.y + box.h - 1 - y;
    const int16_t edgeY      = (distTop < distBottom) ? distTop : distBottom;

    for (int16_t x = xStart; x < xEnd; ++x) {
      const int16_t distLeft  = x - box.x;
      const int16_t distRight = box.x + box.w - 1 - x;
      const int16_t edgeX     = (distLeft < distRight) ? distLeft : distRight;
      uint16_t     &dstPixel  = stripBuf[(x - stripX0) * LCD_HEIGHT + y];

      if (box.radius > 0u && edgeX < box.radius && edgeY < box.radius) {
        rasterRoundedCorner(box, dstPixel, edgeX, edgeY);
      } else {
        const bool     isBorder = (box.strokeRgb != kNoColor) && (edgeX == 0 || edgeY == 0);
        const uint16_t color    = isBorder ? box.strokeRgb : box.fillRgb;
        if (color != kNoColor) {
          dstPixel = color;
        }
      }
    }
  }
}

void rasterBitmapCmd(const BitmapCmd &bmp, int16_t stripX0, uint16_t *stripBuf) {
  int16_t minX = (bmp.x > stripX0) ? bmp.x : stripX0;
  if (minX < bmp.clipX0) {
    minX = bmp.clipX0;
  }

  const int16_t maxX =
      ((bmp.x + bmp.w) < (stripX0 + kStripCols)) ? (bmp.x + bmp.w) : (stripX0 + kStripCols);
  if (minX >= maxX) {
    return;
  }

  for (int16_t sy = 0; sy < bmp.h; ++sy) {
    const int16_t py = bmp.y + sy;
    if (py < 0 || py >= LCD_HEIGHT) {
      continue;
    }

    for (int16_t px = minX; px < maxX; ++px) {
      const uint32_t bitIdx = static_cast<uint32_t>(sy) * bmp.w + static_cast<uint32_t>(px - bmp.x);
      uint8_t        alpha2 = (bmp.data[bitIdx >> 2] >> ((bitIdx & 0x03u) * 2u)) & 0x03u;
      if (bmp.fadeLeftEdge && (px - bmp.clipX0) < 2 && alpha2 > 1u) {
        alpha2 = static_cast<uint8_t>(alpha2 - 1u);
      }

      plotStripPixel(stripBuf, stripX0, px, py, bmp.fgRgb, alpha2BitTo8Bit(alpha2));
    }
  }
}

void rasterTextCmd(const TextCmd &txt, int16_t stripX0, uint16_t *stripBuf) {
  const Hs02UI::Font *font       = Hs02UI::selectFont(txt.pxSize);
  const int16_t       lineStride = static_cast<int16_t>(txt.pxSize + 2u);
  int16_t             penX       = txt.x;
  int16_t             penY       = txt.y;

  for (const uint8_t *p = reinterpret_cast<const uint8_t *>(textPool + txt.textOffset); *p; ++p) {
    if (*p == '\n') {
      penX = txt.x;
      penY = static_cast<int16_t>(penY + lineStride);
      continue;
    }

    const Hs02UI::Glyph *glyph = Hs02UI::findGlyph(font, *p);
    if (!glyph) {
      continue;
    }

    uint32_t bitIdx = 0;
    for (uint8_t gy = 0; gy < glyph->height; ++gy) {
      for (uint8_t gx = 0; gx < glyph->width; ++gx, ++bitIdx) {
        const int16_t px = static_cast<int16_t>(penX + glyph->xOffset + gx);
        if (px < txt.clipX0 || px >= txt.clipX1) {
          continue;
        }

        const uint8_t alpha2 =
            (font->data[glyph->dataOffset + (bitIdx >> 2)] >> ((bitIdx & 0x03u) * 2u)) & 0x03u;
        plotStripPixel(stripBuf, stripX0, px, static_cast<int16_t>(penY + glyph->yOffset + gy),
                       txt.fgRgb, alpha2BitTo8Bit(alpha2));
      }
    }

    penX = static_cast<int16_t>(penX + glyph->advance);
  }
}

void rasterizeStrip(uint8_t stripIndex, uint16_t *stripBuf) {
  const int16_t stripX0 = static_cast<int16_t>(stripIndex * kStripCols);
  for (uint16_t i = 0; i < kStripCols * LCD_HEIGHT; ++i) {
    stripBuf[i] = backgroundRgb;
  }

  for (uint8_t i = 0; i < cmdCount; ++i) {
    const DrawCmd &cmd = drawCmds[i];
    switch (cmd.type) {
    case CmdType::Box:
      rasterBoxCmd(cmd.box, stripX0, stripBuf);
      break;
    case CmdType::Bitmap:
      rasterBitmapCmd(cmd.bitmap, stripX0, stripBuf);
      break;
    case CmdType::Text:
      rasterTextCmd(cmd.text, stripX0, stripBuf);
      break;
    default:
      break;
    }
  }
}

uint32_t computeFrameHash() {
  uint32_t       hash    = kFnvOffsetBasis;
  const uint8_t *bgBytes = reinterpret_cast<const uint8_t *>(&backgroundRgb);
  for (size_t i = 0; i < sizeof(backgroundRgb); ++i) {
    hash = (hash ^ bgBytes[i]) * kFnvPrime;
  }

  const uint8_t *cmdBytes = reinterpret_cast<const uint8_t *>(drawCmds);
  const size_t   cmdLen   = static_cast<size_t>(cmdCount) * sizeof(DrawCmd);
  for (size_t i = 0; i < cmdLen; ++i) {
    hash = (hash ^ cmdBytes[i]) * kFnvPrime;
  }

  for (uint16_t i = 0; i < textPoolLen; ++i) {
    hash = (hash ^ static_cast<uint8_t>(textPool[i])) * kFnvPrime;
  }

  return hash;
}

} // namespace

void LCD::setColorPalette(const uint16_t *palette) {
  activePalette2bpp = palette ? palette : palette2bpp;
  backgroundRgb     = activePalette2bpp[0];
}

void LCD::clearScreenColor() {
  cmdCount      = 0;
  textPoolLen   = 0;
  backgroundRgb = kDefaultBgRgb;
}

void LCD::setBgColorRgb(uint16_t color) { backgroundRgb = color; }

void LCD::setColorMode(bool active) {
  colorModeActive = active;
  displayChecksum = ~0u;
}

void LCD::drawBoxRgb(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb,
                     uint16_t strokeRgb, uint8_t radius) {
  if (w <= 0 || h <= 0 || cmdCount >= kMaxDrawCmds) {
    return;
  }

  uint8_t clampedRadius = (radius > kMaxCornerRad) ? kMaxCornerRad : radius;
  if (clampedRadius * 2 > w) {
    clampedRadius = static_cast<uint8_t>(w / 2);
  }
  if (clampedRadius * 2 > h) {
    clampedRadius = static_cast<uint8_t>(h / 2);
  }

  DrawCmd cmd       = {};
  cmd.type          = CmdType::Box;
  cmd.box.x         = x;
  cmd.box.y         = y;
  cmd.box.w         = w;
  cmd.box.h         = h;
  cmd.box.fillRgb   = fillRgb;
  cmd.box.strokeRgb = strokeRgb;
  cmd.box.radius    = clampedRadius;

  drawCmds[cmdCount++] = cmd;
}

void LCD::drawAaBitmapRgb(const uint8_t *data, uint8_t width, uint8_t height, int16_t x0,
                          int16_t y0, uint16_t fgRgb, int16_t clipX0, int16_t clipX1,
                          bool fadeLeftEdge) {
  (void)clipX1;
  if (!data || width == 0u || height == 0u || cmdCount >= kMaxDrawCmds) {
    return;
  }

  DrawCmd cmd             = {};
  cmd.type                = CmdType::Bitmap;
  cmd.bitmap.data         = data;
  cmd.bitmap.x            = x0;
  cmd.bitmap.y            = y0;
  cmd.bitmap.clipX0       = clipX0;
  cmd.bitmap.fgRgb        = fgRgb;
  cmd.bitmap.w            = width;
  cmd.bitmap.h            = height;
  cmd.bitmap.fadeLeftEdge = fadeLeftEdge;

  drawCmds[cmdCount++] = cmd;
}

int16_t LCD::drawSpaceGroteskRgb(const char *str, int16_t x, int16_t y, uint8_t pxSize,
                                 uint16_t fgRgb, int16_t clipX0, int16_t clipX1) {
  if (!str || textPoolLen + 1u >= kTextPoolSize) {
    return x;
  }

  const uint16_t      offset = textPoolLen;
  const Hs02UI::Font *font   = Hs02UI::selectFont(pxSize);
  uint16_t            width  = 0;
  uint16_t            copied = 0;

  for (const uint8_t *p = reinterpret_cast<const uint8_t *>(str);
       *p && (textPoolLen + copied + 1u < kTextPoolSize); ++p) {
    ++copied;
    if (*p == '\n') {
      width = 0;
    } else {
      const Hs02UI::Glyph *glyph = Hs02UI::findGlyph(font, *p);
      if (glyph) {
        width = static_cast<uint16_t>(width + glyph->advance);
      }
    }
  }

  memcpy(textPool + textPoolLen, str, copied);
  textPoolLen             = static_cast<uint16_t>(textPoolLen + copied);
  textPool[textPoolLen++] = '\0';

  if (cmdCount < kMaxDrawCmds) {
    DrawCmd cmd         = {};
    cmd.type            = CmdType::Text;
    cmd.text.x          = x;
    cmd.text.y          = y;
    cmd.text.clipX0     = clipX0;
    cmd.text.clipX1     = clipX1;
    cmd.text.fgRgb      = fgRgb;
    cmd.text.textOffset = offset;
    cmd.text.pxSize     = pxSize;

    drawCmds[cmdCount++] = cmd;
  }

  return static_cast<int16_t>(x + width);
}

void LCD::renderFrameRgb565(uint16_t *outRowMajor160x80) {
  uint16_t *stripPixels = reinterpret_cast<uint16_t *>(screenBuffer);
  for (uint8_t strip = 0; strip < kStripCount; ++strip) {
    rasterizeStrip(strip, stripPixels);
    for (int16_t lx = 0; lx < kStripCols; ++lx) {
      for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
        outRowMajor160x80[y * LCD_WIDTH + strip * kStripCols + lx] =
            stripPixels[lx * LCD_HEIGHT + y];
      }
    }
  }
}

void LCD::refreshColor(bool force) {
  const uint32_t hash = computeFrameHash();
  if (!colorModeActive || (!force && hash == displayChecksum)) {
    return;
  }

  displayChecksum = hash;
  setDrawingWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
  uint16_t *stripPixels = reinterpret_cast<uint16_t *>(screenBuffer);

  for (uint8_t strip = 0; strip < kStripCount; ++strip) {
    rasterizeStrip(strip, stripPixels);

    // ST7735 runs with INVON enabled and expects big-endian RGB565 in column-major order.
    for (uint16_t i = 0; i < kStripCols * LCD_HEIGHT; ++i) {
      const uint16_t inv        = static_cast<uint16_t>(~stripPixels[i]);
      screenBuffer[i * 2u]      = static_cast<uint8_t>(inv >> 8);
      screenBuffer[i * 2u + 1u] = static_cast<uint8_t>(inv & 0xFFu);
    }

    SPI_CLASS::fastSend(screenBuffer, kStripCols * LCD_HEIGHT * 2u);
  }
}

#endif // LCD_160x80
