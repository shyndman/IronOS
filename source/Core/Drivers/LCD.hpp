/*
 * LCD.hpp
 *
 *  Created on: 27May.,2026
 *      Author: Ben V. Brown <Ralim>
 *      Modified: MrTick, OK2CM
 *      Target ST7735, Fnirsi HS02
 */

#pragma once
#include "configuration.h"
#ifdef LCD_160x80
#include "Font.h"
#include "cmsis_os.h"
#include <BSP.h>
#include <stdbool.h>
#include <string.h>

#include <SPI_Wrapper.hpp>
#define SPI_CLASS FRToSSPI

#define LCD_WIDTH  160
#define LCD_HEIGHT 80

#define LCD_SCREEN_BUF_SIZE      ((LCD_WIDTH * LCD_HEIGHT) / 8)
#define LCD_SCREEN_BUF_SIZE_2BPP ((LCD_WIDTH * LCD_HEIGHT * 2) / 8)
#define LCD_SCREEN_BUF_SIZE_4BPP ((LCD_WIDTH * LCD_HEIGHT * 4) / 8)

#define ST7735_XOFFSET 1
#define ST7735_YOFFSET 26

class LCD {
public:
  static void initialize();
  static void refresh(const bool force = false) {
    if (force || checkDisplayBufferChecksum()) {
      refreshColor();
    }
  }

  static void setColorMode(bool active);
  static bool isColorMode() { return colorModeActive; }
  static void setColorPalette(const uint16_t *palette) { activePalette2bpp = palette ? palette : palette2bpp; }

  static void clearScreenColor() { memset(activeColorBuffer, 0, LCD_SCREEN_BUF_SIZE_4BPP); }

  static void fillRect2bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t colorIndex);
  static void drawRoundedBar4bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t rampIndex);
  static void drawRing2bpp(uint8_t cx, uint8_t cy, uint8_t r, uint8_t thickness, uint8_t colorIndex, float startAngle, float endAngle);
  static void drawTick2bpp(uint8_t cx, uint8_t cy, float angle, uint8_t rInner, uint8_t rOuter, uint8_t colorIndex);
  static void drawGlyph2bpp(uint16_t charCode, FontStyle fontStyle, uint8_t x, uint8_t y, uint8_t colorIndex);
  static void drawTextColor(const char *str, uint8_t x, uint8_t y, FontStyle fontStyle, uint8_t colorIndex, uint8_t maxChars = 255);
  static void drawBitmap2bpp(const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t x, uint8_t y, uint8_t colorIndex);
  static void drawAaBitmap2bpp(const uint8_t *packed2bpp, uint8_t width, uint8_t height, int16_t x0, int16_t y0, uint8_t rampIndex, int16_t clipX0 = 0, int16_t clipX1 = LCD_WIDTH);
  static int16_t drawSpaceGroteskText(const char *str, int16_t x, int16_t y, uint8_t pxSize, uint8_t rampIndex, int16_t clipX0 = 0, int16_t clipX1 = LCD_WIDTH);
  static uint16_t measureSpaceGroteskText(const char *str, uint8_t pxSize);

  static void setDisplayState(bool state);

  // Set the rotation for the screen
  static void setRotation(bool leftHanded, bool refresh = true);
  static void setBrightness(uint8_t brightness);
  static void setInverse(bool inverted);

  // Clears the buffer
  static void clearScreen() { clearScreenColor(); }

  // Blit a raw big-endian RGB565 image straight to the panel, bypassing the framebuffer.
  // Used for the colour boot logo; the next refresh() overwrites it.
  static void drawNativeImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *rgb565);
  static void drawIndexedImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *indices, const uint16_t *invertedPalette);
  static void drawArea(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t *ptr);        // Draw an area, but y must be aligned on 0/8 offset
  static void drawAreaSwapped(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t *ptr); // Draw an area, but y must be aligned on 0/8 offset
  static void fillArea(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t value);       // Fill an area, but y must be aligned on 0/8 offset
  static void drawFilledRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool clear);

  static void flushSecondBuffer(void);
  static void useSecondaryFramebuffer(bool useSecondary);

  static bool scrollDown(uint8_t pos);
  static bool scrollUp(uint8_t pos);
  static bool scrollHorizontal(const bool dirForward, uint16_t progress, uint8_t offset);
  static void setFramebuffer(uint8_t *buffer);

private:
  static bool checkDisplayBufferChecksum() {
    static_assert(LCD_SCREEN_BUF_SIZE_4BPP % 4 == 0, "4bpp framebuffer size must be a multiple of 4");
    uint32_t        hash      = 0;
    const uint32_t  len       = LCD_SCREEN_BUF_SIZE_4BPP / 4;
    const uint32_t *pBuffer   = (const uint32_t *)activeColorBuffer;
    uint32_t        wordsLeft = len;
    while (wordsLeft > 0) {
      hash += (wordsLeft * (*pBuffer++));
      wordsLeft--;
    }

    bool result     = hash != displayChecksum;
    displayChecksum = hash;
    return result;
  }
  static void drawChar(uint16_t charCode, FontStyle fontStyle, const uint8_t soft_x_limit);
  static void setDrawingWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

  static void setPixel4bpp(int16_t x, int16_t y, uint8_t paletteIdx) {
    if (x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
      return;
    }
    uint16_t byteIdx = (uint16_t)y * (LCD_WIDTH / 2) + ((uint16_t)x >> 1);
    uint8_t  shift   = ((uint8_t)x & 1u) * 4u;
    activeColorBuffer[byteIdx] = (activeColorBuffer[byteIdx] & ~(0x0Fu << shift)) | ((paletteIdx & 0x0Fu) << shift);
  }
  static void blendPixelRamp4bpp(int16_t x, int16_t y, uint8_t rampIndex, uint8_t alpha2bit) {
    if (alpha2bit == 0 || x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
      return;
    }
    uint16_t      byteIdx  = (uint16_t)y * (LCD_WIDTH / 2) + ((uint16_t)x >> 1);
    uint8_t       shift    = ((uint8_t)x & 1u) * 4u;
    const uint8_t existing = (activeColorBuffer[byteIdx] >> shift) & 0x0Fu;
    const uint8_t target   = ((rampIndex & 0x03u) << 2) | (alpha2bit & 0x03u);
    if ((existing >> 2) == (rampIndex & 0x03u) && (existing & 0x03u) > alpha2bit) {
      return;
    }
    activeColorBuffer[byteIdx] = (activeColorBuffer[byteIdx] & ~(0x0Fu << shift)) | (target << shift);
  }
  static void setPixel2bpp(uint8_t x, uint8_t y, uint8_t colorIndex) {
    if (colorIndex == 0) {
      setPixel4bpp(x, y, 0);
    } else {
      setPixel4bpp(x, y, ((colorIndex - 1) << 2) | 3u);
    }
  }
  static uint8_t *monoSecondFrameBuffer() { return &screenBuffer[LCD_SCREEN_BUF_SIZE]; }
  static void     refreshColor();
  // Plots one radial segment at `angle`, covering radii [rInner, rOuter]. Shared by drawRing2bpp
  // and drawTick2bpp.
  static void plotRadialSegment(uint8_t cx, uint8_t cy, float angle, uint8_t rInner, uint8_t rOuter, uint8_t colorIndex);

  static uint8_t *stripPointers[LCD_HEIGHT / 8]; // Pointers to the strips to allow for buffer having extra content
  static uint32_t displayChecksum;
  static bool     colorModeActive;
  static uint8_t  screenBuffer[LCD_SCREEN_BUF_SIZE_4BPP];
  static uint8_t *colorPrimaryBuffer;
  static uint8_t *colorSecondaryBuffer;
  static uint8_t *activeColorBuffer;
  static uint8_t  loopCounter;

  static const uint16_t  palette2bpp[16];
  static const uint16_t *activePalette2bpp;
};

#endif // LCD_160x80
