/*
 * LCD.cpp
 *
 *  Created on: 27May.,2026
 *      Author: Ben V. Brown
 *      Modified: MrTick, OK2CM
 *      Target ST7735, Fnirsi HS02
 */
#include "LCD.hpp"
#ifdef LCD_160x80

#include "Hs02UiAssets.hpp"
#include "LCD_Port.hpp"
#include "Pins.h"
#include "Settings.h"
#include "Translation.h"
#include "cmsis_os.h"
#include "configuration.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// rendering to the buffer
uint8_t *LCD::stripPointers[LCD_HEIGHT / 8]; // Pointers to the strips to allow for buffer having extra content

alignas(uint32_t) uint8_t LCD::screenBuffer[LCD_SCREEN_BUF_SIZE_4BPP];
uint8_t *LCD::colorPrimaryBuffer   = LCD::screenBuffer;
uint8_t *LCD::colorSecondaryBuffer = LCD::screenBuffer;
uint8_t *LCD::activeColorBuffer    = LCD::screenBuffer;
uint32_t LCD::displayChecksum;
bool     LCD::colorModeActive = true;
uint8_t  LCD::loopCounter;

const uint16_t LCD::palette2bpp[16] = {
    0x10A3, 0x5B2B, 0xA6B5, 0xF7BE,
    0x10A3, 0x3370, 0x55FD, 0x7EBF,
    0x10A3, 0x2A69, 0x4C51, 0x6BF2,
    0x10A3, 0x1124, 0x19A5, 0x1926,
};
const uint16_t *LCD::activePalette2bpp = Hs02UI::kPaletteHomeIdle;
// ST7735 Commands
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A
#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// LCD rotation
const FRToSSPI::SPI_CMD lcdInitCmdRotR = {ST7735_MADCTL, FRToSSPI::SPI_CMD_PAYLOAD, 1, (uint8_t[]){0x88}};
const FRToSSPI::SPI_CMD lcdInitCmdRotL = {ST7735_MADCTL, FRToSSPI::SPI_CMD_PAYLOAD, 1, (uint8_t[]){0x48}};

// LCD initialization
const FRToSSPI::SPI_CMD lcdInitCmds[] = {
    {ST7735_SWRESET, FRToSSPI::SPI_CMD_DELAY_MS, 150,                                                                                                        NULL},
    { ST7735_SLPOUT, FRToSSPI::SPI_CMD_DELAY_MS, 200,                                                                                                        NULL},

    {ST7735_FRMCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                                                                               (uint8_t[]){0x05, 0x3A, 0x3A}},
    {ST7735_FRMCTR2,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                                                                               (uint8_t[]){0x05, 0x3A, 0x3A}},
    {ST7735_FRMCTR3,  FRToSSPI::SPI_CMD_PAYLOAD,   6,                                                             (uint8_t[]){0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A}},

    { ST7735_PWCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                                                                               (uint8_t[]){0x62, 0x02, 0x04}},
    { ST7735_PWCTR2,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                                                                                           (uint8_t[]){0xC0}},
    { ST7735_PWCTR3,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                                                                                     (uint8_t[]){0x0D, 0x00}},
    { ST7735_PWCTR4,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                                                                                     (uint8_t[]){0x8D, 0x6A}},
    { ST7735_PWCTR5,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                                                                                     (uint8_t[]){0x8D, 0xEE}},

    {ST7735_GMCTRP1,  FRToSSPI::SPI_CMD_PAYLOAD,  16, (uint8_t[]){0x10, 0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D, 0x0E, 0x10}},
    {ST7735_GMCTRN1,  FRToSSPI::SPI_CMD_PAYLOAD,  16, (uint8_t[]){0x10, 0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D, 0x0E, 0x10}},

    { ST7735_INVCTR,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                                                                                           (uint8_t[]){0x03}},
    {  ST7735_INVON,  FRToSSPI::SPI_CMD_PAYLOAD,   0,                                                                                                        NULL},
    { ST7735_VMCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                                                                                           (uint8_t[]){0x0E}},
    lcdInitCmdRotR,
    { ST7735_COLMOD,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                                                                                           (uint8_t[]){0x05}},

    {  ST7735_NORON, FRToSSPI::SPI_CMD_DELAY_MS,  10,                                                                                                        NULL},
    { ST7735_DISPON, FRToSSPI::SPI_CMD_DELAY_MS, 100,                                                                                                        NULL},
};

FRToSSPI::SPI_CMD lcdSetAreaCmds[] = {
    {ST7735_RASET, FRToSSPI::SPI_CMD_PAYLOAD, 4,  (uint8_t[]){0, ST7735_XOFFSET, 0, LCD_WIDTH + ST7735_XOFFSET - 1}},
    {ST7735_CASET, FRToSSPI::SPI_CMD_PAYLOAD, 4, (uint8_t[]){0, ST7735_YOFFSET, 0, LCD_HEIGHT + ST7735_YOFFSET - 1}},
    {ST7735_RAMWR, FRToSSPI::SPI_CMD_PAYLOAD, 0,                                                               NULL},
};

void LCD::drawNativeImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *rgb565) {
  setDrawingWindow(x, y, w, h);
  SPI_CLASS::fastSend(rgb565, (size_t)w * h * 2);
}

void LCD::drawIndexedImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *indices, const uint16_t *invertedPalette) {
  static uint8_t colRGB[LCD_HEIGHT * 2];
  setDrawingWindow(x, y, w, h);
  for (uint8_t col = 0; col < w; col++) {
    const uint8_t *colIndices = &indices[(uint16_t)col * h];
    for (uint8_t row = 0; row < h; row++) {
      const uint16_t rgb  = invertedPalette[colIndices[row]];
      colRGB[row * 2]     = (uint8_t)(rgb >> 8);
      colRGB[row * 2 + 1] = (uint8_t)(rgb & 0xFF);
    }
    SPI_CLASS::fastSend(colRGB, (size_t)h * 2);
  }
}
void LCD::setDrawingWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  lcdSetAreaCmds[0].data[1] = x + ST7735_XOFFSET;
  lcdSetAreaCmds[0].data[3] = x + w + ST7735_XOFFSET - 1;
  lcdSetAreaCmds[1].data[1] = y + ST7735_YOFFSET;
  lcdSetAreaCmds[1].data[3] = y + h + ST7735_YOFFSET - 1;

  FRToSSPI::sendCmdChain(lcdSetAreaCmds, sizeof(lcdSetAreaCmds) / sizeof(*lcdSetAreaCmds));
}

/*
 * Animation timing function that follows a bezier curve.
 * @param t A given percentage value [0..<100]
 * Returns a new percentage value with ease in and ease out.
 * Original floating point formula: t * t * (3.0f - 2.0f * t);
 */
static uint16_t easeInOutTiming(uint16_t t) { return t * t * (300 - 2 * t) / 10000; }

/*
 * Returns the value between a and b, using a percentage value t.
 * @param a The value associated with 0%
 * @param b The value associated with 100%
 * @param t The percentage [0..<100]
 */
static uint16_t lerp(uint16_t a, uint16_t b, uint16_t t) { return a + t * (b - a) / 100; }

void LCD::initialize() {
  for (uint8_t i = 0; i < LCD_HEIGHT / 8; i++) {
    stripPointers[i] = &screenBuffer[i * LCD_WIDTH];
  }

  FRToSSPI::init();
  FRToSSPI::sendCmdChain(lcdInitCmds, sizeof(lcdInitCmds) / sizeof(*lcdInitCmds));

  // Keep the backlight off until the first meaningful frame is ready. The boot
  // logo is a full-screen image, so an intermediate GRAM clear is unnecessary.
}

void LCD::setFramebuffer(uint8_t *buffer) {
  for (uint8_t i = 0; i < LCD_HEIGHT / 8; i++) {
    stripPointers[i] = &buffer[i * LCD_WIDTH];
  }
}

void LCD::setColorMode(bool active) {
  colorModeActive   = active;
  activeColorBuffer = colorPrimaryBuffer;
  displayChecksum   = ~0U;
}

bool LCD::scrollHorizontal(const bool, uint16_t, uint8_t) { return false; }
void LCD::useSecondaryFramebuffer(bool) {}
bool LCD::scrollDown(uint8_t) { return false; }
bool LCD::scrollUp(uint8_t) { return false; }

void LCD::setRotation(bool leftHanded, bool refresh) {
  if (leftHanded) {
    FRToSSPI::sendCmdChain(&lcdInitCmdRotL, 1);
  } else {
    FRToSSPI::sendCmdChain(&lcdInitCmdRotR, 1);
  }
  if (refresh) {
    LCD::refresh(true);
  }
}

void LCD::setBrightness(uint8_t brightness) { LCDSetBacklight(brightness); }

void LCD::setDisplayState(bool state) {
  static bool hasTurnedOff = false;
  const FRToSSPI::SPI_CMD cmdDisp = {(uint8_t)(state ? ST7735_DISPON : ST7735_DISPOFF), FRToSSPI::SPI_CMD_PAYLOAD, 0, NULL};
  if (state) {
    if (hasTurnedOff) {
      FRToSSPI::sendCmdChain(&cmdDisp, 1);
      LCDSetBacklight(getSettingValue(SettingsOptions::DisplayBrightness));
    }
    GPIO_SetBits(LED_Port, LED1_Pin);
  } else {
    hasTurnedOff = true;
    LCDSetBacklight(0);
    FRToSSPI::sendCmdChain(&cmdDisp, 1);
    GPIO_ResetBits(LED_Port, LED1_Pin);
  }
}
void LCD::setInverse(bool inverse) {
  const FRToSSPI::SPI_CMD cmdInvSet = {(uint8_t)(inverse ? ST7735_INVON : ST7735_INVOFF), FRToSSPI::SPI_CMD_PAYLOAD, 0, NULL};
  FRToSSPI::sendCmdChain(&cmdInvSet, 1);
}

void LCD::flushSecondBuffer(void) { displayChecksum = ~0U; }

// Draw an area, but y must be aligned on 0/8 offset
void LCD::drawArea(int16_t x, int8_t y, uint8_t width, uint8_t height, const uint8_t *ptr) {
  // Splat this from x->x+width in two strides
  if (x <= -width) {
    return; // cutoffleft
  }
  if (x > LCD_WIDTH) {
    return; // cutoff right
  }

  uint8_t visibleStart = 0;
  uint8_t visibleEnd   = width;

  // trimming to draw partials
  if (x < 0) {
    visibleStart -= x; // subtract negative value == add absolute value
  }
  if (x + width > LCD_WIDTH) {
    visibleEnd = LCD_WIDTH - x;
  }
  uint8_t rowsDrawn = 0;
  while (height > 0) {
    for (uint8_t xx = visibleStart; xx < visibleEnd; xx++) {
      stripPointers[(y / 8) + rowsDrawn][x + xx] = ptr[xx + (rowsDrawn * width)];
    }
    height -= 8;
    rowsDrawn++;
  }
}

// Draw an area, but y must be aligned on 0/8 offset
// For data which has octets swapped in a 16-bit word.
void LCD::drawAreaSwapped(int16_t x, int8_t y, uint8_t width, uint8_t height, const uint8_t *ptr) {
  // Splat this from x->x+width in two strides
  if (x <= -width) {
    return; // cutoffleft
  }
  if (x > LCD_WIDTH) {
    return; // cutoff right
  }

  uint8_t visibleStart = 0;
  uint8_t visibleEnd   = width;

  // trimming to draw partials
  if (x < 0) {
    visibleStart -= x; // subtract negative value == add absolute value
  }
  if (x + width > LCD_WIDTH) {
    visibleEnd = LCD_WIDTH - x;
  }

  uint8_t rowsDrawn = 0;
  while (height > 0) {
    for (uint8_t xx = visibleStart; xx < visibleEnd; xx += 2) {
      stripPointers[(y / 8) + rowsDrawn][x + xx]     = ptr[xx + 1 + (rowsDrawn * width)];
      stripPointers[(y / 8) + rowsDrawn][x + xx + 1] = ptr[xx + (rowsDrawn * width)];
    }
    height -= 8;
    rowsDrawn++;
  }
}

void LCD::fillArea(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t value) {
  // Splat this from x->x+wide in two strides
  if (x <= -wide) {
    return; // cutoffleft
  }
  if (x > LCD_WIDTH) {
    return; // cutoff right
  }

  uint8_t visibleStart = 0;
  uint8_t visibleEnd   = wide;

  // trimming to draw partials
  if (x < 0) {
    visibleStart -= x; // subtract negative value == add absolute value
  }
  if (x + wide > LCD_WIDTH) {
    visibleEnd = LCD_WIDTH - x;
  }

  uint8_t rowsDrawn = 0;
  while (height > 0) {
    for (uint8_t xx = visibleStart; xx < visibleEnd; xx++) {
      stripPointers[(y / 8) + rowsDrawn][x + xx] = value;
    }
    height -= 8;
    rowsDrawn++;
  }
}

// ---- 2bpp colour screen support --------------------------------------------------------------
// This variant is only ever built for the 160x80 colour panel, so the SMALL/LARGE glyph cell
// sizes are fixed constants here, matching Display.hpp's LCD_160x80 branch.
namespace {
constexpr uint8_t kSmallW = 12, kSmallH = 16;
constexpr uint8_t kLargeW = 24, kLargeH = 32;
} // namespace

void LCD::refreshColor() {
  static uint8_t colRGB[LCD_HEIGHT * 2];
  setDrawingWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
  for (uint16_t x = 0; x < LCD_WIDTH; x++) {
    const uint8_t shift = (x & 1u) * 4u;
    const uint8_t xByte = (uint8_t)(x >> 1);
    for (uint16_t y = 0; y < LCD_HEIGHT; y++) {
      uint8_t  packed   = activeColorBuffer[y * (LCD_WIDTH / 2) + xByte];
      uint8_t  index    = (packed >> shift) & 0x0Fu;
      uint16_t rgb      = (uint16_t)~activePalette2bpp[index];
      colRGB[y * 2]     = (uint8_t)(rgb >> 8);
      colRGB[y * 2 + 1] = (uint8_t)(rgb & 0xFF);
    }
    SPI_CLASS::fastSend(colRGB, sizeof(colRGB));
  }
}

void LCD::fillRect2bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t colorIndex) {
  for (uint8_t y = y0; y < y0 + h && y < LCD_HEIGHT; y++) {
    for (uint8_t x = x0; x < x0 + w && x < LCD_WIDTH; x++) {
      setPixel2bpp(x, y, colorIndex);
    }
  }
}

void LCD::drawRoundedBar4bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t rampIndex) {
  if (w == 0 || h == 0) {
    return;
  }
  for (uint8_t dy = 0; dy < h; dy++) {
    const bool outerRow = (h >= 4) && (dy == 0 || dy == h - 1);
    for (uint8_t dx = 0; dx < w; dx++) {
      const bool outerCol = (w >= 4) && (dx == 0 || dx == w - 1);
      uint8_t    alpha    = 3;
      if (outerRow && outerCol) {
        alpha = 1;
      } else if (outerRow && (dx == 1 || dx + 2 == w)) {
        alpha = 2;
      }
      blendPixelRamp4bpp((int16_t)x0 + dx, (int16_t)y0 + dy, rampIndex, alpha);
    }
  }
}

void LCD::drawAaBitmap2bpp(const uint8_t *packed2bpp, uint8_t width, uint8_t height, int16_t x0, int16_t y0, uint8_t rampIndex, int16_t clipX0, int16_t clipX1) {
  uint32_t pixelIdx = 0;
  for (uint8_t dy = 0; dy < height; dy++) {
    const int16_t py = y0 + dy;
    for (uint8_t dx = 0; dx < width; dx++, pixelIdx++) {
      const int16_t px = x0 + dx;
      if (px < clipX0 || px >= clipX1 || py < 0 || py >= LCD_HEIGHT) {
        continue;
      }
      uint8_t alpha = (packed2bpp[pixelIdx >> 2] >> ((pixelIdx & 3u) * 2u)) & 0x03u;
      if (alpha) {
        if (clipX0 == 4 && px <= 10) {
          alpha = (uint8_t)((alpha * (px - 3) + 3) / 8);
        }
        if (alpha) {
          blendPixelRamp4bpp(px, py, rampIndex, alpha);
        }
      }
    }
  }
}

namespace {
const Hs02UI::Font *selectFont(uint8_t pxSize) {
  switch (pxSize) {
  case 15:
    return &Hs02UI::kFont15;
  case 17:
    return &Hs02UI::kFont17;
  case 30:
    return &Hs02UI::kFont30;
  case 19:
  default:
    return &Hs02UI::kFont19;
  }
}

const Hs02UI::Glyph *findGlyph(const Hs02UI::Font *font, uint8_t code) {
  uint8_t lo = 0, hi = font->glyphCount;
  while (lo < hi) {
    const uint8_t mid = (lo + hi) / 2;
    const uint8_t c   = font->glyphs[mid].code;
    if (c == code) {
      return &font->glyphs[mid];
    }
    if (c < code) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return nullptr;
}
} // namespace

uint16_t LCD::measureSpaceGroteskText(const char *str, uint8_t pxSize) {
  if (!str) {
    return 0;
  }
  const Hs02UI::Font *font  = selectFont(pxSize);
  uint16_t            width = 0;
  for (const uint8_t *p = reinterpret_cast<const uint8_t *>(str); *p && *p != '\n'; ++p) {
    const Hs02UI::Glyph *g = findGlyph(font, *p);
    if (g) {
      width += g->advance;
    }
  }
  return width;
}

int16_t LCD::drawSpaceGroteskText(const char *str, int16_t x, int16_t y, uint8_t pxSize, uint8_t rampIndex, int16_t clipX0, int16_t clipX1) {
  if (!str) {
    return x;
  }
  const Hs02UI::Font *font       = selectFont(pxSize);
  const int16_t       startX     = x;
  const uint8_t       lineStride = (pxSize == 17) ? 22 : ((pxSize == 15) ? 17 : (pxSize + 4));
  for (const uint8_t *p = reinterpret_cast<const uint8_t *>(str); *p; ++p) {
    if (*p == '\n') {
      x = startX;
      y += lineStride;
      continue;
    }
    const Hs02UI::Glyph *g = findGlyph(font, *p);
    if (!g) {
      continue;
    }
    if (g->width > 0 && g->height > 0) {
      drawAaBitmap2bpp(&font->data[g->dataOffset], g->width, g->height, x + g->xOffset, y + g->yOffset, rampIndex, clipX0, clipX1);
    }
    x += g->advance;
  }
  return x;
}
void LCD::plotRadialSegment(uint8_t cx, uint8_t cy, float angle, uint8_t rInner, uint8_t rOuter, uint8_t colorIndex) {
  const float c = cosf(angle), s = sinf(angle);
  for (uint8_t r = rInner; r <= rOuter; r++) {
    int16_t x = (int16_t)(cx + c * r);
    int16_t y = (int16_t)(cy + s * r);
    if (x >= 0 && y >= 0 && x < LCD_WIDTH && y < LCD_HEIGHT) {
      setPixel2bpp((uint8_t)x, (uint8_t)y, colorIndex);
    }
  }
}

void LCD::drawRing2bpp(uint8_t cx, uint8_t cy, uint8_t r, uint8_t thickness, uint8_t colorIndex, float startAngle, float endAngle) {
  const uint8_t   rInner       = (thickness / 2 >= r) ? 0 : r - thickness / 2;
  const uint8_t   rOuter       = r + thickness / 2;
  const uint16_t  innerSquared = rInner * rInner;
  const uint16_t  outerSquared = rOuter * rOuter;
  constexpr float kTwoPi       = 6.28318531f;

  // Rasterise the annular sector directly. Sampling radial spokes leaves holes
  // after float-to-integer truncation; testing every pixel guarantees a solid
  // ring on the actual 160x80 framebuffer.
  for (int16_t y = (int16_t)cy - rOuter; y <= (int16_t)cy + rOuter; y++) {
    for (int16_t x = (int16_t)cx - rOuter; x <= (int16_t)cx + rOuter; x++) {
      const int16_t  dx = x - cx, dy = y - cy;
      const uint16_t distanceSquared = dx * dx + dy * dy;
      if (distanceSquared < innerSquared || distanceSquared > outerSquared) {
        continue;
      }
      float angle = atan2f((float)dy, (float)dx);
      if (angle < 0.0f) {
        angle += kTwoPi;
      }
      if (angle < startAngle) {
        angle += kTwoPi;
      }
      if (angle >= startAngle && angle <= endAngle && x >= 0 && y >= 0 && x < LCD_WIDTH && y < LCD_HEIGHT) {
        setPixel2bpp((uint8_t)x, (uint8_t)y, colorIndex);
      }
    }
  }
}

void LCD::drawTick2bpp(uint8_t cx, uint8_t cy, float angle, uint8_t rInner, uint8_t rOuter, uint8_t colorIndex) { plotRadialSegment(cx, cy, angle, rInner, rOuter, colorIndex); }

void LCD::drawGlyph2bpp(uint16_t charCode, FontStyle fontStyle, uint8_t x, uint8_t y, uint8_t colorIndex) {
  (void)charCode;
  (void)fontStyle;
  (void)x;
  (void)y;
  (void)colorIndex;
}

void LCD::drawTextColor(const char *str, uint8_t x, uint8_t y, FontStyle fontStyle, uint8_t colorIndex, uint8_t maxChars) {
  const uint8_t *next      = reinterpret_cast<const uint8_t *>(str);
  const uint8_t  fontWidth = (fontStyle == FontStyle::SMALL) ? kSmallW : (fontStyle == FontStyle::LARGE ? kLargeW : kSmallW);
  while (*next && maxChars--) {
    uint16_t index;
    if (*next <= 0xF0) {
      index = *next;
      next++;
    } else {
      if (!next[1]) {
        return;
      }
      index = (next[0] - 0xF0) * 0xFF - 15 + next[1];
      next += 2;
    }
    drawGlyph2bpp(index, fontStyle, x, y, colorIndex);
    x += fontWidth;
  }
}

void LCD::drawBitmap2bpp(const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t x, uint8_t y, uint8_t colorIndex) {
  const uint8_t strips = (height + 7) / 8;
  for (uint8_t strip = 0; strip < strips; strip++) {
    for (uint8_t gx = 0; gx < width; gx++) {
      const uint8_t bits = bitmap[strip * width + gx];
      for (uint8_t gy = 0; gy < 8; gy++) {
        const uint8_t py = strip * 8 + gy;
        if (py < height && (bits & (1u << gy))) {
          setPixel2bpp(x + gx, y + py, colorIndex);
        }
      }
    }
  }
}

void LCD::drawFilledRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool clear) {
  // Ensure coordinates are within bounds
  if (x0 >= LCD_WIDTH || y0 >= LCD_HEIGHT || x1 >= LCD_WIDTH || y1 >= LCD_HEIGHT) {
    return;
  }

  // Calculate the height in rows
  uint8_t startRow  = y0 / 8;
  uint8_t endRow    = y1 / 8;
  uint8_t startMask = 0xFF << (y0 % 8);
  uint8_t endMask   = 0xFF >> (7 - (y1 % 8));

  for (uint8_t row = startRow; row <= endRow; row++) {
    uint8_t mask = 0xFF;
    if (row == startRow) {
      mask &= startMask;
    }
    if (row == endRow) {
      mask &= endMask;
    }

    for (uint8_t x = x0; x <= x1; x++) {
      if (clear) {
        stripPointers[row][x] &= ~mask;
      } else {
        stripPointers[row][x] |= mask;
      }
    }
  }
}

#endif // LCD_160x80
