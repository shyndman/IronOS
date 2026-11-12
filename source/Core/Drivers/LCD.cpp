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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t *LCD::stripPointers[LCD_HEIGHT / 8];

alignas(uint32_t) uint8_t LCD::screenBuffer[LCD_SCREEN_BUF_SIZE_4BPP];
uint8_t *LCD::colorPrimaryBuffer   = LCD::screenBuffer;
uint8_t *LCD::colorSecondaryBuffer = LCD::screenBuffer;
uint8_t *LCD::activeColorBuffer    = LCD::screenBuffer;
uint32_t LCD::displayChecksum;
bool     LCD::colorModeActive = true;
uint8_t  LCD::loopCounter;

const uint16_t LCD::palette2bpp[16] = {
    0x10A3, 0x5B2B, 0xA6B5, 0xF7BE, 0x10A3, 0x3370, 0x55FD, 0x7EBF,
    0x10A3, 0x2A69, 0x4C51, 0x6BF2, 0x10A3, 0x1124, 0x19A5, 0x1926,
};
const uint16_t *LCD::activePalette2bpp = Hs02UI::kPaletteHomeIdle;

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

const FRToSSPI::SPI_CMD lcdInitCmdRotR = {ST7735_MADCTL, FRToSSPI::SPI_CMD_PAYLOAD, 1,
                                          (uint8_t[]){0x88}};
const FRToSSPI::SPI_CMD lcdInitCmdRotL = {ST7735_MADCTL, FRToSSPI::SPI_CMD_PAYLOAD, 1,
                                          (uint8_t[]){0x48}};

const FRToSSPI::SPI_CMD lcdInitCmds[] = {
    {ST7735_SWRESET, FRToSSPI::SPI_CMD_DELAY_MS, 150,NULL                                                     },
    { ST7735_SLPOUT, FRToSSPI::SPI_CMD_DELAY_MS, 200,                                            NULL},
    {ST7735_FRMCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                   (uint8_t[]){0x05, 0x3A, 0x3A}},
    {ST7735_FRMCTR2,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                   (uint8_t[]){0x05, 0x3A, 0x3A}},
    {ST7735_FRMCTR3,  FRToSSPI::SPI_CMD_PAYLOAD,   6, (uint8_t[]){0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A}},
    { ST7735_PWCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   3,                   (uint8_t[]){0x62, 0x02, 0x04}},
    { ST7735_PWCTR2,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                               (uint8_t[]){0xC0}},
    { ST7735_PWCTR3,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                         (uint8_t[]){0x0D, 0x00}},
    { ST7735_PWCTR4,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                         (uint8_t[]){0x8D, 0x6A}},
    { ST7735_PWCTR5,  FRToSSPI::SPI_CMD_PAYLOAD,   2,                         (uint8_t[]){0x8D, 0xEE}},
    {ST7735_GMCTRP1,  FRToSSPI::SPI_CMD_PAYLOAD,  16,
     (uint8_t[]){0x10, 0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D,
     0x0E, 0x10}                                                                                     },
    {ST7735_GMCTRN1,  FRToSSPI::SPI_CMD_PAYLOAD,  16,
     (uint8_t[]){0x10, 0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D,
     0x0E, 0x10}                                                                                     },
    { ST7735_INVCTR,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                               (uint8_t[]){0x03}},
    {  ST7735_INVON,  FRToSSPI::SPI_CMD_PAYLOAD,   0,                                            NULL},
    { ST7735_VMCTR1,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                               (uint8_t[]){0x0E}},
    lcdInitCmdRotR,
    { ST7735_COLMOD,  FRToSSPI::SPI_CMD_PAYLOAD,   1,                               (uint8_t[]){0x05}},
    {  ST7735_NORON, FRToSSPI::SPI_CMD_DELAY_MS,  10,                                            NULL},
    { ST7735_DISPON, FRToSSPI::SPI_CMD_DELAY_MS, 100,                                            NULL},
};

FRToSSPI::SPI_CMD lcdSetAreaCmds[] = {
    {ST7735_RASET, FRToSSPI::SPI_CMD_PAYLOAD, 4,
     (uint8_t[]){0, ST7735_XOFFSET, 0, LCD_WIDTH + ST7735_XOFFSET - 1} },
    {ST7735_CASET, FRToSSPI::SPI_CMD_PAYLOAD, 4,
     (uint8_t[]){0, ST7735_YOFFSET, 0, LCD_HEIGHT + ST7735_YOFFSET - 1}},
    {ST7735_RAMWR, FRToSSPI::SPI_CMD_PAYLOAD, 0,                   NULL},
};

void LCD::drawNativeImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *rgb565) {
  setDrawingWindow(x, y, w, h);
  SPI_CLASS::fastSend(rgb565, static_cast<size_t>(w) * h * 2u);
}

void LCD::drawIndexedImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *indices,
                           const uint16_t *invertedPalette) {
  static uint8_t colRGB[LCD_HEIGHT * 2];
  setDrawingWindow(x, y, w, h);
  for (uint8_t col = 0; col < w; col++) {
    const uint8_t *colIndices = &indices[static_cast<uint16_t>(col) * h];
    for (uint8_t row = 0; row < h; row++) {
      const uint16_t rgb  = invertedPalette[colIndices[row]];
      colRGB[row * 2]     = static_cast<uint8_t>(rgb >> 8);
      colRGB[row * 2 + 1] = static_cast<uint8_t>(rgb & 0xFFu);
    }
    SPI_CLASS::fastSend(colRGB, static_cast<size_t>(h) * 2u);
  }
}

void LCD::setDrawingWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  lcdSetAreaCmds[0].data[1] = static_cast<uint8_t>(x + ST7735_XOFFSET);
  lcdSetAreaCmds[0].data[3] = static_cast<uint8_t>(x + w + ST7735_XOFFSET - 1);
  lcdSetAreaCmds[1].data[1] = static_cast<uint8_t>(y + ST7735_YOFFSET);
  lcdSetAreaCmds[1].data[3] = static_cast<uint8_t>(y + h + ST7735_YOFFSET - 1);
  FRToSSPI::sendCmdChain(lcdSetAreaCmds, sizeof(lcdSetAreaCmds) / sizeof(*lcdSetAreaCmds));
}

void LCD::initialize() {
  for (uint8_t i = 0; i < LCD_HEIGHT / 8; i++) {
    stripPointers[i] = &screenBuffer[i * LCD_WIDTH];
  }
  FRToSSPI::init();
  FRToSSPI::sendCmdChain(lcdInitCmds, sizeof(lcdInitCmds) / sizeof(*lcdInitCmds));
}

void LCD::setFramebuffer(uint8_t *buffer) {
  for (uint8_t i = 0; i < LCD_HEIGHT / 8; i++) {
    stripPointers[i] = &buffer[i * LCD_WIDTH];
  }
}

bool LCD::scrollHorizontal(const bool, uint16_t, uint8_t) { return false; }
void LCD::useSecondaryFramebuffer(bool) {}
bool LCD::scrollDown(uint8_t) { return false; }
bool LCD::scrollUp(uint8_t) { return false; }

void LCD::setRotation(bool leftHanded, bool refresh) {
  FRToSSPI::sendCmdChain(leftHanded ? &lcdInitCmdRotL : &lcdInitCmdRotR, 1);
  if (refresh) {
    LCD::refresh(true);
  }
}

void LCD::setBrightness(uint8_t brightness) { LCDSetBacklight(brightness); }

void LCD::setDisplayState(bool state) {
  static bool             hasTurnedOff = false;
  const FRToSSPI::SPI_CMD cmdDisp = {static_cast<uint8_t>(state ? ST7735_DISPON : ST7735_DISPOFF),
                                     FRToSSPI::SPI_CMD_PAYLOAD, 0, NULL};
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
  const FRToSSPI::SPI_CMD cmdInvSet = {static_cast<uint8_t>(inverse ? ST7735_INVON : ST7735_INVOFF),
                                       FRToSSPI::SPI_CMD_PAYLOAD, 0, NULL};
  FRToSSPI::sendCmdChain(&cmdInvSet, 1);
}

void LCD::flushSecondBuffer(void) { displayChecksum = ~0U; }
void LCD::drawArea(int16_t, int8_t, uint8_t, uint8_t, const uint8_t *) {}
void LCD::drawAreaSwapped(int16_t, int8_t, uint8_t, uint8_t, const uint8_t *) {}
void LCD::fillArea(int16_t, int8_t, uint8_t, uint8_t, const uint8_t) {}

namespace {
constexpr uint16_t kNoColor = 0x0001u;
uint16_t           legacyRampColor(uint8_t rampIndex) {
  return LCD::activePalette2bpp[((rampIndex & 0x03u) << 2) | 0x03u];
}
} // namespace

void LCD::drawAaBitmap2bpp(const uint8_t *packed2bpp, uint8_t width, uint8_t height, int16_t x0,
                           int16_t y0, uint8_t rampIndex, int16_t clipX0, int16_t clipX1) {
  drawAaBitmapRgb(packed2bpp, width, height, x0, y0, legacyRampColor(rampIndex), clipX0, clipX1,
                  clipX0 == 4);
}

void LCD::drawRoundedBar4bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t rampIndex) {
  drawBoxRgb(x0, y0, w, h, legacyRampColor(rampIndex), kNoColor, static_cast<uint8_t>(h / 2u));
}

void LCD::fillRect2bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t colorIndex) {
  const uint16_t color = colorIndex ? activePalette2bpp[colorIndex & 0x0Fu] : kNoColor;
  drawBoxRgb(x0, y0, w, h, color, kNoColor);
}

int16_t LCD::drawSpaceGroteskText(const char *str, int16_t x, int16_t y, uint8_t pxSize,
                                  uint8_t rampIndex, int16_t clipX0, int16_t clipX1) {
  return drawSpaceGroteskRgb(str, x, y, pxSize, legacyRampColor(rampIndex), clipX0, clipX1);
}

void LCD::drawGlyph2bpp(uint16_t, FontStyle, uint8_t, uint8_t, uint8_t) {}
void LCD::drawTick2bpp(uint8_t, uint8_t, float, uint8_t, uint8_t, uint8_t) {}
void LCD::drawRing2bpp(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, float, float) {}
void LCD::drawBitmap2bpp(const uint8_t *, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}

void LCD::drawTextColor(const char *str, uint8_t x, uint8_t y, FontStyle fontStyle,
                        uint8_t colorIndex, uint8_t maxChars) {
  char     clipped[64];
  uint16_t len = 0;

  while (str && str[len] && len < maxChars && len + 1u < sizeof(clipped)) {
    clipped[len] = str[len];
    ++len;
  }

  clipped[len] = '\0';
  drawSpaceGroteskRgb(clipped, x, y, fontStyle == FontStyle::LARGE ? 19u : 15u,
                      legacyRampColor(colorIndex));
}

void LCD::drawFilledRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool clear) {
  if (x1 >= x0 && y1 >= y0) {
    const int16_t  w     = static_cast<int16_t>(x1 - x0 + 1);
    const int16_t  h     = static_cast<int16_t>(y1 - y0 + 1);
    const uint16_t color = clear ? activePalette2bpp[0] : activePalette2bpp[15];
    drawBoxRgb(x0, y0, w, h, color, kNoColor);
  }
}

uint16_t LCD::measureSpaceGroteskText(const char *str, uint8_t pxSize) {
  if (!str) {
    return 0;
  }

  const Hs02UI::Font *font  = Hs02UI::selectFont(pxSize);
  uint16_t            width = 0;
  for (const uint8_t *p = reinterpret_cast<const uint8_t *>(str); *p && *p != '\n'; ++p) {
    const Hs02UI::Glyph *glyph = Hs02UI::findGlyph(font, *p);
    if (glyph) {
      width = static_cast<uint16_t>(width + glyph->advance);
    }
  }

  return width;
}

#endif // LCD_160x80
