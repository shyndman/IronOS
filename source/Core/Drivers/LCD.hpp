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
#include <SPI_Wrapper.hpp>
#include <stdbool.h>
#include <string.h>
#define SPI_CLASS FRToSSPI

#define LCD_WIDTH                160
#define LCD_HEIGHT               80
#define LCD_SCREEN_BUF_SIZE      ((LCD_WIDTH * LCD_HEIGHT) / 8)
#define LCD_SCREEN_BUF_SIZE_2BPP ((LCD_WIDTH * LCD_HEIGHT * 2) / 8)
#define LCD_SCREEN_BUF_SIZE_4BPP ((20 * LCD_HEIGHT * 16) / 8)
#define ST7735_XOFFSET           1
#define ST7735_YOFFSET           26

class LCD {
public:
  static void initialize();
  static void refresh(const bool force = false) { refreshColor(force); }
  static void setColorMode(bool active);
  static bool isColorMode() { return colorModeActive; }
  static void setColorPalette(const uint16_t *palette);
  static void clearScreenColor();
  static void setBgColorRgb(uint16_t bgRgb);
  static void drawBoxRgb(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb,
                         uint16_t strokeRgb = 0x0001u, uint8_t radius = 0);
  static void drawAaBitmapRgb(const uint8_t *packed2bpp, uint8_t width, uint8_t height, int16_t x0,
                              int16_t y0, uint16_t fgRgb, int16_t clipX0 = 0,
                              int16_t clipX1 = LCD_WIDTH, bool fadeLeftEdge = false);
  static int16_t drawSpaceGroteskRgb(const char *str, int16_t x, int16_t y, uint8_t pxSize,
                                     uint16_t fgRgb, int16_t clipX0 = 0,
                                     int16_t clipX1 = LCD_WIDTH);
  static void    renderFrameRgb565(uint16_t *outRowMajor160x80);

  static void fillRect2bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t colorIndex);
  static void drawRoundedBar4bpp(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t rampIndex);
  static void drawRing2bpp(uint8_t cx, uint8_t cy, uint8_t r, uint8_t thickness, uint8_t colorIndex,
                           float startAngle, float endAngle);
  static void drawTick2bpp(uint8_t cx, uint8_t cy, float angle, uint8_t rInner, uint8_t rOuter,
                           uint8_t colorIndex);
  static void drawGlyph2bpp(uint16_t charCode, FontStyle fontStyle, uint8_t x, uint8_t y,
                            uint8_t colorIndex);
  static void drawTextColor(const char *str, uint8_t x, uint8_t y, FontStyle fontStyle,
                            uint8_t colorIndex, uint8_t maxChars = 255);
  static void drawBitmap2bpp(const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t x,
                             uint8_t y, uint8_t colorIndex);
  static void drawAaBitmap2bpp(const uint8_t *packed2bpp, uint8_t width, uint8_t height, int16_t x0,
                               int16_t y0, uint8_t rampIndex, int16_t clipX0 = 0,
                               int16_t clipX1 = LCD_WIDTH);
  static int16_t  drawSpaceGroteskText(const char *str, int16_t x, int16_t y, uint8_t pxSize,
                                       uint8_t rampIndex, int16_t clipX0 = 0,
                                       int16_t clipX1 = LCD_WIDTH);
  static uint16_t measureSpaceGroteskText(const char *str, uint8_t pxSize);
  static void     setDisplayState(bool state);
  static void     setRotation(bool leftHanded, bool refresh = true);
  static void     setBrightness(uint8_t brightness);
  static void     setInverse(bool inverted);
  static void     clearScreen() { clearScreenColor(); }
  static void drawNativeImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *rgb565);
  static void drawIndexedImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *indices,
                               const uint16_t *invertedPalette);
  static void drawArea(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t *ptr);
  static void drawAreaSwapped(int16_t x, int8_t y, uint8_t wide, uint8_t height,
                              const uint8_t *ptr);
  static void fillArea(int16_t x, int8_t y, uint8_t wide, uint8_t height, const uint8_t value);
  static void drawFilledRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool clear);
  static void flushSecondBuffer(void);
  static void useSecondaryFramebuffer(bool useSecondary);
  static bool scrollDown(uint8_t pos);
  static bool scrollUp(uint8_t pos);
  static bool scrollHorizontal(const bool dirForward, uint16_t progress, uint8_t offset);
  static void setFramebuffer(uint8_t *buffer);

public:
  static void            setDrawingWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
  static void            refreshColor(bool force = false);
  static uint8_t         screenBuffer[LCD_SCREEN_BUF_SIZE_4BPP];
  static uint8_t        *stripPointers[LCD_HEIGHT / 8];
  static uint32_t        displayChecksum;
  static bool            colorModeActive;
  static uint8_t        *colorPrimaryBuffer;
  static uint8_t        *colorSecondaryBuffer;
  static uint8_t        *activeColorBuffer;
  static uint8_t         loopCounter;
  static const uint16_t  palette2bpp[16];
  static const uint16_t *activePalette2bpp;
};

#endif // LCD_160x80
