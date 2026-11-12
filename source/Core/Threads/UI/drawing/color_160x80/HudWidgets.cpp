/*
 * HudWidgets.cpp — Implementation of stateless 160x80 Chaos Telemetry HUD widgets.
 *
 * Purpose:
 *   Implements reusable UI primitives (optical baseline centering, pill measurement,
 *   keycap paddles, detent strips, and gate-clipped segment bars) so individual
 *   screen files express intent rather than repeating coordinate arithmetic.
 *
 * Why drawSplitSegmentBar exists:
 *   Continuous gauges show a 2px vertical gate marker at the exact value coordinate.
 *   Lighting whole 10px segments by coarse bucket index causes solid bars to overhang
 *   past the gate marker. Splitting a straddled segment directly at gateX guarantees
 *   solid fill never protrudes beyond the indicator line.
 */

#include "HudWidgets.hpp"

#ifdef LCD_160x80

#include <limits.h>

namespace ChaosHud {
namespace {

constexpr int16_t kSegmentGap      = 2;
constexpr int16_t kGateWidth       = 2;
constexpr int16_t kDetentGap       = 3;
constexpr int16_t kPaddleCapWidth  = 20;
constexpr uint8_t kThermalZoneBins = 12;

int16_t clampInt16(int32_t value) {
  if (value < INT16_MIN) {
    return INT16_MIN;
  }

  if (value > INT16_MAX) {
    return INT16_MAX;
  }

  return static_cast<int16_t>(value);
}

void drawGateMarker(int16_t barY, int16_t barH, int16_t gateX, uint16_t gateColor) {
  if (gateX >= 0) {
    const int16_t markerY = static_cast<int16_t>(barY - 2);
    const int16_t markerH = static_cast<int16_t>(barH + 4);
    Display::drawBoxRgb(gateX, markerY, kGateWidth, markerH, gateColor, Color::kNone, 0);
  }
}

} // namespace

const CategoryTheme &categoryTheme(uint16_t categoryIndex) {
  static constexpr CategoryTheme kThemes[5] = {
      {  Color::kPwr,   Color::kPwrDim},
      { Color::kHeat,  Color::kHeatDim},
      {  Color::kSlp,   Color::kSlpDim},
      {Color::kVolts, Color::kVoltsDim},
      {  Color::kAdv,   Color::kAdvDim},
  };

  return kThemes[categoryIndex < 5u ? categoryIndex : 0u];
}

char *appendUnsigned(char *dst, uint32_t value) {
  char    reversed[10];
  uint8_t count = 0;

  do {
    reversed[count++] = static_cast<char>('0' + (value % 10u));
    value /= 10u;
  } while (value > 0u && count < sizeof(reversed));

  while (count > 0u) {
    *dst++ = reversed[--count];
  }

  *dst = '\0';
  return dst;
}

char *appendSigned(char *dst, int32_t value) {
  if (value < 0) {
    *dst++             = '-';
    const uint32_t mag = static_cast<uint32_t>(-(value + 1)) + 1u;
    return appendUnsigned(dst, mag);
  }

  return appendUnsigned(dst, static_cast<uint32_t>(value));
}

char *appendStr(char *dst, const char *src) {
  if (src) {
    while (*src) {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
  return dst;
}

char *appendTenths(char *dst, uint32_t x10Value, const char *suffix) {
  dst    = appendUnsigned(dst, x10Value / 10u);
  *dst++ = '.';
  *dst++ = static_cast<char>('0' + (x10Value % 10u));
  *dst   = '\0';
  return appendStr(dst, suffix);
}

// Space Grotesk cap ink sits at yOffset 4..15 (15px), 5..17 (17px), 5..19 (19px), and 9..30 (30px).
// Centering against (2*yOffset + capHeight) aligns caps and digits to the optical vertical midline.
int16_t centeredTextY(int16_t boxY, int16_t boxH, uint8_t pxSize) {
  int16_t opticalSpan = 19;

  if (pxSize == 17u) {
    opticalSpan = 22;
  } else if (pxSize == 19u) {
    opticalSpan = 24;
  } else if (pxSize == 30u) {
    opticalSpan = 39;
  }

  return static_cast<int16_t>(boxY + (boxH - opticalSpan) / 2);
}

int16_t drawTextRight(const char *str, int16_t rightX, int16_t y, uint8_t pxSize, uint16_t fgRgb) {
  const int16_t width = static_cast<int16_t>(Display::measureSpaceGrotesk(str, pxSize));
  const int16_t drawX = static_cast<int16_t>(rightX - width);

  Display::drawSpaceGroteskRgb(str, drawX, y, pxSize, fgRgb);
  return drawX;
}

int16_t drawTextCentered(const char *str, int16_t x, int16_t w, int16_t y, uint8_t pxSize,
                         uint16_t fgRgb) {
  const int16_t width = static_cast<int16_t>(Display::measureSpaceGrotesk(str, pxSize));
  const int16_t drawX = static_cast<int16_t>(x + (w - width) / 2);

  Display::drawSpaceGroteskRgb(str, drawX, y, pxSize, fgRgb, x, static_cast<int16_t>(x + w));
  return drawX;
}

void drawSolidRail(uint16_t fgRgb) {
  Display::drawBoxRgb(0, 0, Layout::kRailW, Layout::kScreenH, fgRgb, Color::kNone, 0);
}

void drawSegmentedRail(uint8_t stepCount, uint8_t activeStepIndex, uint16_t activeRgb,
                       uint16_t dimRgb) {
  if (stepCount == 0u) {
    return;
  }

  const int16_t totalGap   = static_cast<int16_t>((stepCount - 1u) * kSegmentGap);
  const int16_t usableSpan = static_cast<int16_t>(Layout::kScreenH - totalGap);
  int16_t       y          = 0;

  for (uint8_t i = 0; i < stepCount; ++i) {
    const int16_t pipH =
        static_cast<int16_t>((usableSpan * (i + 1u)) / stepCount - (usableSpan * i) / stepCount);
    const uint16_t color = (i == activeStepIndex) ? activeRgb : dimRgb;

    Display::drawBoxRgb(0, y, Layout::kRailW, pipH, color, Color::kNone, 0);
    y = static_cast<int16_t>(y + pipH + kSegmentGap);
  }
}

void drawHazardStripe(uint16_t stripeRgb, uint16_t dimRgb) {
  for (uint8_t i = 0; i < 10u; ++i) {
    const int16_t  dashX = static_cast<int16_t>(6 + i * 15);
    const uint16_t color = ((i & 1u) != 0u) ? dimRgb : stripeRgb;

    Display::drawBoxRgb(dashX, 0, 12, 3, color, Color::kNone, 0);
  }
}

void drawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
             uint8_t radius) {
  Display::drawBoxRgb(x, y, w, h, fillRgb, strokeRgb, radius);
}

int16_t measurePillWidth(const char *label, uint8_t pxSize, int16_t padX) {
  return clampInt16(static_cast<int32_t>(Display::measureSpaceGrotesk(label, pxSize)) + 2 * padX);
}

void drawPill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
              const char *label, uint16_t textRgb, uint8_t pxSize) {
  drawBox(x, y, w, h, fillRgb, strokeRgb, 4);
  const int16_t inset = (strokeRgb == Color::kNone) ? 1 : 2;
  drawTextCentered(label, static_cast<int16_t>(x + inset), static_cast<int16_t>(w - 2 * inset),
                   centeredTextY(y, h, pxSize), pxSize, textRgb);
}

int16_t drawPillRight(int16_t rightX, int16_t y, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
                      const char *label, uint16_t textRgb, int16_t padX) {
  const int16_t pillW = measurePillWidth(label, 15, padX);
  const int16_t pillX = static_cast<int16_t>(rightX - pillW);

  drawPill(pillX, y, pillW, h, fillRgb, strokeRgb, label, textRgb, 15);
  return pillX;
}

void drawIconPill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fillRgb, uint16_t strokeRgb,
                  const Hs02UI::Icon2bpp &icon, uint16_t iconRgb, const char *label,
                  uint16_t textRgb) {
  drawBox(x, y, w, h, fillRgb, strokeRgb, 4);

  const int16_t labelW = static_cast<int16_t>(Display::measureSpaceGrotesk(label, 15));
  const int16_t pairW  = static_cast<int16_t>(icon.width + 4 + labelW);
  const int16_t startX = static_cast<int16_t>(x + (w - pairW) / 2);
  const int16_t iconY  = static_cast<int16_t>(y + (h - icon.height) / 2);
  const int16_t clipX0 = static_cast<int16_t>(x + 1);
  const int16_t clipX1 = static_cast<int16_t>(x + w - 1);

  Display::drawAaBitmapRgb(icon.data, icon.width, icon.height, startX, iconY, iconRgb, clipX0,
                           clipX1);
  Display::drawSpaceGroteskRgb(label, static_cast<int16_t>(startX + icon.width + 4),
                               centeredTextY(y, h, 15), 15, textRgb, clipX0, clipX1);
}

int16_t segmentWidth(int16_t totalW, uint8_t count) {
  if (count == 0u) {
    return 0;
  }

  return static_cast<int16_t>((totalW - (count - 1u) * kSegmentGap) / count);
}

int16_t segmentLeftX(int16_t barX, int16_t totalW, uint8_t count, uint8_t index) {
  const int16_t segW = segmentWidth(totalW, count);
  return static_cast<int16_t>(barX + index * (segW + kSegmentGap));
}

void drawSegmentBar(int16_t x, int16_t baseBottomY, int16_t totalW, uint8_t count,
                    const uint8_t *heights, const uint16_t *colors, int16_t gate1X,
                    uint16_t gate1Rgb, int16_t gate2X, uint16_t gate2Rgb, uint8_t gateH) {
  if (count == 0u) {
    return;
  }

  const int16_t segW = segmentWidth(totalW, count);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t segX = static_cast<int16_t>(x + i * (segW + kSegmentGap));
    const int16_t segY = static_cast<int16_t>(baseBottomY - heights[i]);
    Display::drawBoxRgb(segX, segY, segW, heights[i], colors[i], Color::kNone, 1);
  }

  if (gateH > 0u) {
    const int16_t gateTopY = static_cast<int16_t>(baseBottomY - gateH + 2);
    drawGateMarker(gateTopY, static_cast<int16_t>(gateH - 4), gate1X, gate1Rgb);
    drawGateMarker(gateTopY, static_cast<int16_t>(gateH - 4), gate2X, gate2Rgb);
  }
}

void drawSplitSegmentBar(int16_t x, int16_t baseBottomY, int16_t totalW, uint8_t count,
                         int16_t gateX, uint8_t litHeight, const uint16_t *litColors,
                         uint8_t unlitHeight, const uint16_t *unlitColors, uint16_t gateRgb,
                         uint8_t gateH, int16_t gate2X, uint16_t gate2Rgb) {
  if (count == 0u) {
    return;
  }

  const int16_t segW   = segmentWidth(totalW, count);
  const int16_t litY   = static_cast<int16_t>(baseBottomY - litHeight);
  const int16_t unlitY = static_cast<int16_t>(baseBottomY - unlitHeight);

  for (uint8_t i = 0; i < count; ++i) {
    const int16_t segX     = static_cast<int16_t>(x + i * (segW + kSegmentGap));
    const int16_t segRight = static_cast<int16_t>(segX + segW);

    if (gateX >= segRight) {
      Display::drawBoxRgb(segX, litY, segW, litHeight, litColors[i], Color::kNone, 1);
    } else if (gateX <= segX) {
      Display::drawBoxRgb(segX, unlitY, segW, unlitHeight, unlitColors[i], Color::kNone, 1);
    } else {
      const int16_t leftW  = static_cast<int16_t>(gateX - segX);
      const int16_t rightX = static_cast<int16_t>(gateX + kGateWidth);
      const int16_t rightW = static_cast<int16_t>(segRight - rightX);

      Display::drawBoxRgb(segX, litY, leftW, litHeight, litColors[i], Color::kNone, 1);
      if (rightW > 0) {
        Display::drawBoxRgb(rightX, unlitY, rightW, unlitHeight, unlitColors[i], Color::kNone, 1);
      }
    }
  }

  if (gateH > 0u) {
    const int16_t gateTopY = static_cast<int16_t>(baseBottomY - gateH + 2);
    drawGateMarker(gateTopY, static_cast<int16_t>(gateH - 4), gateX, gateRgb);
    drawGateMarker(gateTopY, static_cast<int16_t>(gateH - 4), gate2X, gate2Rgb);
  }
}

void drawUniformBar(int16_t x, int16_t y, int16_t totalW, int16_t h, uint8_t count,
                    const uint16_t *colors, int16_t gate1X, uint16_t gate1Rgb, int16_t gate2X,
                    uint16_t gate2Rgb) {
  if (count == 0u) {
    return;
  }

  const int16_t segW = segmentWidth(totalW, count);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t segX = static_cast<int16_t>(x + i * (segW + kSegmentGap));
    Display::drawBoxRgb(segX, y, segW, h, colors[i], Color::kNone, 1);
  }

  drawGateMarker(y, h, gate1X, gate1Rgb);
  drawGateMarker(y, h, gate2X, gate2Rgb);
}

int16_t mapRangeToBarX(int32_t value, int32_t minVal, int32_t maxVal, int16_t barX, int16_t barW) {
  if (maxVal <= minVal) {
    return barX;
  }

  if (value < minVal) {
    value = minVal;
  }

  if (value > maxVal) {
    value = maxVal;
  }

  return clampInt16(barX + ((value - minVal) * barW) / (maxVal - minVal));
}

uint16_t thermalZoneColor(uint8_t segIdx, uint8_t segCount) {
  if (segCount == 0u) {
    return Color::kVolts;
  }

  const uint16_t bin = static_cast<uint16_t>(segIdx) * kThermalZoneBins / segCount;
  if (bin < 3u) {
    return Color::kVolts;
  }

  if (bin < 6u) {
    return Color::kLock;
  }

  if (bin < 9u) {
    return Color::kWatts;
  }

  if (bin < 11u) {
    return Color::kHeat;
  }

  return Color::kRedline;
}

void drawDetentStrip(int16_t x, int16_t y, int16_t totalW, int16_t h, const char *const *labels,
                     uint8_t count, uint8_t activeIdx, uint16_t activeFillRgb,
                     uint16_t activeStrokeRgb, uint16_t activeTextRgb) {
  if (count == 0u) {
    return;
  }

  const int16_t cellW = static_cast<int16_t>((totalW - (count - 1u) * kDetentGap) / count);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t  cellX    = static_cast<int16_t>(x + i * (cellW + kDetentGap));
    const bool     selected = (i == activeIdx);
    const uint16_t fill     = selected ? activeFillRgb : Color::kCell;
    const uint16_t stroke   = selected ? activeStrokeRgb : Color::kBorder;
    const uint16_t fg       = selected ? activeTextRgb : Color::kMuted;

    drawBox(cellX, y, cellW, h, fill, stroke, 3);
    drawTextCentered(labels[i], cellX, cellW, centeredTextY(y, h, 15), 15, fg);
  }
}

void drawKeyPaddle(int16_t x, int16_t y, int16_t w, int16_t h, const char *capLabel,
                   uint16_t capFillRgb, uint16_t capTextRgb, uint16_t bodyFillRgb,
                   uint16_t strokeRgb, const char *actionLabel, uint16_t actionTextRgb) {
  drawBox(x, y, w, h, bodyFillRgb, strokeRgb, 4);
  drawBox(x, y, kPaddleCapWidth, h, capFillRgb, Color::kNone, 4);
  drawBox(static_cast<int16_t>(x + kPaddleCapWidth - 5), y, 5, h, capFillRgb, Color::kNone, 0);

  const int16_t textY = centeredTextY(y, h, 15);
  drawTextCentered(capLabel, x, kPaddleCapWidth, textY, 15, capTextRgb);
  drawTextCentered(actionLabel, static_cast<int16_t>(x + kPaddleCapWidth),
                   static_cast<int16_t>(w - kPaddleCapWidth), textY, 15, actionTextRgb);
}

void drawChevrons(uint16_t chevronRgb) {
  Display::drawAaBitmapRgb(Hs02UI::kIconChevronLeft.data, Hs02UI::kIconChevronLeft.width,
                           Hs02UI::kIconChevronLeft.height, Layout::kChevronLeftX,
                           Layout::kChevronY, chevronRgb);
  Display::drawAaBitmapRgb(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width,
                           Hs02UI::kIconChevronRight.height, Layout::kChevronRightX,
                           Layout::kChevronY, chevronRgb);
}

void drawSettingBottomRow(uint16_t chevronRgb, const char *valueStr, const char *pillStr,
                          uint16_t pillFillRgb, uint16_t pillStrokeRgb, uint16_t pillTextRgb) {
  constexpr int16_t kLeftBound  = 23;
  constexpr int16_t kRightBound = 138;

  drawChevrons(chevronRgb);
  if (!pillStr || !pillStr[0]) {
    Display::drawSpaceGroteskRgb(valueStr, kLeftBound, 49, 19, Color::kInk, 22, kRightBound);
    return;
  }

  int16_t pillW   = measurePillWidth(pillStr, 15, 5);
  uint8_t valFont = 19;
  int16_t valW    = static_cast<int16_t>(Display::measureSpaceGrotesk(valueStr, valFont));

  if (kLeftBound + valW + 5 + pillW > kRightBound) {
    pillW = measurePillWidth(pillStr, 15, 4);
  }

  if (kLeftBound + valW + 4 + pillW > kRightBound) {
    valFont = 17;
    valW    = static_cast<int16_t>(Display::measureSpaceGrotesk(valueStr, valFont));
  }

  if (kLeftBound + valW + 4 + pillW > kRightBound) {
    pillW = static_cast<int16_t>(kRightBound - kLeftBound - valW - 4);
    if (pillW < 24) {
      pillW = 24;
    }
  }

  const int16_t pillX = static_cast<int16_t>(kRightBound - pillW);
  const int16_t valY  = (valFont == 19u) ? 49 : 51;

  Display::drawSpaceGroteskRgb(valueStr, kLeftBound, valY, valFont, Color::kInk, 22,
                               static_cast<int16_t>(pillX - 3));
  drawPill(pillX, 51, pillW, 19, pillFillRgb, pillStrokeRgb, pillStr, pillTextRgb);
}

} // namespace ChaosHud

#endif // LCD_160x80
