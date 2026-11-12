/*
 * HudWidgets.hpp — Reusable 160x80 Chaos Telemetry HUD primitives and tokens.
 *
 * Purpose:
 *   Provides pure, stateless layout constants, RGB565 theme palettes, zero-allocation
 *   ASCII/UTF-8 string builders, and composite UI widgets (left rails, pills, paddles,
 *   detent selectors, and gate-clipped segment bars) shared across all HS-02 screens.
 *
 * Boundary:
 *   This module knows only about geometry, typography, and color — never about
 *   soldering physics, settings indices, or hardware sensors. Live sensor/setting
 *   queries live in HudTelemetry.hpp, and screen-specific layouts live in draw_*.cpp.
 */

#pragma once

#include "Display.hpp"
#include "Hs02UiAssets.hpp"
#include <stddef.h>
#include <stdint.h>

#ifdef LCD_160x80

namespace ChaosHud {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((static_cast<uint16_t>(r) & 0xF8u) << 8) |
                               ((static_cast<uint16_t>(g) & 0xFCu) << 3) |
                               (static_cast<uint16_t>(b) >> 3));
}

namespace Color {

constexpr uint16_t kBg      = rgb565(0x0E, 0x13, 0x19);
constexpr uint16_t kCell    = rgb565(0x15, 0x1C, 0x25);
constexpr uint16_t kCellHi  = rgb565(0x1E, 0x27, 0x34);
constexpr uint16_t kBorder  = rgb565(0x2A, 0x36, 0x47);
constexpr uint16_t kTrack   = rgb565(0x21, 0x2B, 0x38);
constexpr uint16_t kInk     = rgb565(0xF4, 0xF4, 0xF0);
constexpr uint16_t kMuted   = rgb565(0x7E, 0x8E, 0x9F);
constexpr uint16_t kDarkInk = rgb565(0x0B, 0x10, 0x15);

constexpr uint16_t kPwr        = rgb565(0xED, 0xBF, 0x6F);
constexpr uint16_t kPwrDim     = rgb565(0x2B, 0x22, 0x13);
constexpr uint16_t kHeat       = rgb565(0xFF, 0x78, 0x49);
constexpr uint16_t kHeatDim    = rgb565(0x38, 0x1F, 0x18);
constexpr uint16_t kLock       = rgb565(0x2C, 0xE5, 0x9B);
constexpr uint16_t kLockDim    = rgb565(0x11, 0x2C, 0x22);
constexpr uint16_t kWatts      = rgb565(0xFF, 0xB5, 0x47);
constexpr uint16_t kWattsDim   = rgb565(0x2B, 0x21, 0x12);
constexpr uint16_t kVolts      = rgb565(0x5C, 0xE1, 0xE6);
constexpr uint16_t kVoltsDim   = rgb565(0x11, 0x26, 0x2E);
constexpr uint16_t kSlp        = rgb565(0x9E, 0xA8, 0xFF);
constexpr uint16_t kSlpDim     = rgb565(0x17, 0x1C, 0x36);
constexpr uint16_t kAdv        = rgb565(0xF1, 0x9C, 0xC4);
constexpr uint16_t kAdvDim     = rgb565(0x2F, 0x17, 0x27);
constexpr uint16_t kRedline    = rgb565(0xFF, 0x5C, 0x77);
constexpr uint16_t kRedlineDim = rgb565(0x35, 0x17, 0x22);

constexpr uint16_t kNone = 0x0001u;

} // namespace Color

namespace Layout {

constexpr int16_t kScreenW       = 160;
constexpr int16_t kScreenH       = 80;
constexpr int16_t kRailW         = 3;
constexpr int16_t kContentX      = 10;
constexpr int16_t kRightEdgeX    = 153;
constexpr int16_t kContentW      = kRightEdgeX - kContentX;
constexpr int16_t kBarW          = 142;
constexpr int16_t kGateSpanW     = 140;
constexpr int16_t kTitleOneLineY = 5;
constexpr int16_t kTitleTwoLineY = 4;
constexpr int16_t kBottomRowY    = 53;
constexpr int16_t kChevronLeftX  = 7;
constexpr int16_t kChevronRightX = 140;
constexpr int16_t kChevronY      = 54;
constexpr int16_t kValueX        = 27;
constexpr int16_t kValueY        = 53;

} // namespace Layout

struct CategoryTheme {
  uint16_t fg;
  uint16_t dim;
};

const CategoryTheme &categoryTheme(uint16_t categoryIndex);

char *appendUnsigned(char *dst, uint32_t value);
char *appendSigned(char *dst, int32_t value);
char *appendStr(char *dst, const char *src);
char *appendTenths(char *dst, uint32_t x10Value, const char *suffix);

int16_t centeredTextY(int16_t boxY, int16_t boxH, uint8_t pxSize);
int16_t drawTextRight(const char *str, int16_t rightX, int16_t y, uint8_t pxSize, uint16_t fgRgb);
int16_t drawTextCentered(const char *str, int16_t x, int16_t w, int16_t y, uint8_t pxSize,
                         uint16_t fgRgb);

void drawSolidRail(uint16_t fgRgb);
void drawSegmentedRail(uint8_t stepCount, uint8_t activeStepIndex, uint16_t activeRgb,
                       uint16_t dimRgb);
void drawHazardStripe(uint16_t stripeRgb, uint16_t dimRgb);

void    drawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb,
                uint16_t strokeRgb = Color::kNone, uint8_t radius = 4);
int16_t measurePillWidth(const char *label, uint8_t pxSize = 15, int16_t padX = 5);
void    drawPill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
                 const char *label, uint16_t textRgb, uint8_t pxSize = 15);
int16_t drawPillRight(int16_t rightX, int16_t y, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
                      const char *label, uint16_t textRgb, int16_t padX = 5);
void drawIconPill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
                  const Hs02UI::Icon2bpp &icon, uint16_t iconRgb, const char *label,
                  uint16_t textRgb);

int16_t segmentLeftX(int16_t barX, int16_t totalW, uint8_t count, uint8_t index);
int16_t segmentWidth(int16_t totalW, uint8_t count);
void    drawSegmentBar(int16_t x, int16_t baseBottomY, int16_t totalW, uint8_t count,
                       const uint8_t *heights, const uint16_t *colors, int16_t gate1X = -1,
                       uint16_t gate1Rgb = Color::kInk, int16_t gate2X = -1,
                       uint16_t gate2Rgb = Color::kInk, uint8_t gateH = 12);
void    drawSplitSegmentBar(int16_t x, int16_t baseBottomY, int16_t totalW, uint8_t count,
                            int16_t gateX, uint8_t litHeight, const uint16_t *litColors,
                            uint8_t unlitHeight, const uint16_t *unlitColors,
                            uint16_t gateRgb = Color::kInk, uint8_t gateH = 14, int16_t gate2X = -1,
                            uint16_t gate2Rgb = Color::kInk);
void    drawUniformBar(int16_t x, int16_t y, int16_t totalW, int16_t h, uint8_t count,
                       const uint16_t *colors, int16_t gate1X = -1, uint16_t gate1Rgb = Color::kInk,
                       int16_t gate2X = -1, uint16_t gate2Rgb = Color::kInk);
int16_t mapRangeToBarX(int32_t value, int32_t minVal, int32_t maxVal, int16_t barX, int16_t barW);
uint16_t thermalZoneColor(uint8_t segIdx, uint8_t segCount);

void drawDetentStrip(int16_t x, int16_t y, int16_t totalW, int16_t h, const char *const *labels,
                     uint8_t count, uint8_t activeIdx, uint16_t activeFillRgb,
                     uint16_t activeStrokeRgb, uint16_t activeTextRgb);
void drawKeyPaddle(int16_t x, int16_t y, int16_t w, int16_t h, const char *capLabel,
                   uint16_t capFillRgb, uint16_t capTextRgb, uint16_t bodyFillRgb,
                   uint16_t strokeRgb, const char *actionLabel, uint16_t actionTextRgb);
void drawChevrons(uint16_t chevronRgb);
void drawSettingBottomRow(uint16_t chevronRgb, const char *valueStr, const char *pillStr = nullptr,
                          uint16_t pillFillRgb   = Color::kNone,
                          uint16_t pillStrokeRgb = Color::kNone,
                          uint16_t pillTextRgb   = Color::kInk);

} // namespace ChaosHud

#endif // LCD_160x80
