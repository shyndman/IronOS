/*
 * draw_vin_calibration.cpp — Interactive input-voltage divider trim screen for HS-02.
 *
 * Purpose:
 *   Renders the live "VIN trim" vernier sub-screen entered from Settings -> Advanced ->
 *   Calibrate input voltage, displaying real-time input voltage against VoltageDiv offset.
 *
 * Why it exists:
 *   Isolates the interactive calibration sub-loop UI from static Advanced setting cards.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

namespace ChaosHud {
namespace {

constexpr int32_t kNominalDivisor = 540;
constexpr int32_t kMaxOffset      = 15;
constexpr int16_t kVernierX       = 10;
constexpr int16_t kVernierBaseY   = 40;
constexpr int16_t kVernierW       = 142;
constexpr uint8_t kVernierCount   = 11;
constexpr uint8_t kCenterTick     = 5;

int32_t clampedDivisorOffset(uint16_t divisor) {
  int32_t offset = static_cast<int32_t>(divisor) - kNominalDivisor;
  if (offset < -kMaxOffset) {
    offset = -kMaxOffset;
  }

  if (offset > kMaxOffset) {
    offset = kMaxOffset;
  }

  return offset;
}

void drawVoltagePill(uint16_t voltageX10) {
  Display::drawBoxRgb(83, 4, 69, 20, Color::kVoltsDim, Color::kVolts, 4);
  Display::drawBoxRgb(90, 12, 5, 5, Color::kVolts, Color::kNone);

  char value[16];
  appendTenths(value, voltageX10, " V");
  Display::drawSpaceGroteskRgb(value, 99, 6, 15, Color::kVolts);
}

void drawVernier(int32_t offset) {
  uint8_t       heights[kVernierCount];
  uint16_t      colors[kVernierCount];
  const int32_t step       = (offset > 0) ? ((offset + 1) / 2) : ((offset - 1) / 2);
  const int32_t activeTick = static_cast<int32_t>(kCenterTick) + (offset == 0 ? 0 : step);

  for (uint8_t i = 0; i < kVernierCount; ++i) {
    const int32_t tick    = i;
    const bool    between = (offset > 0 && tick >= kCenterTick && tick < activeTick) ||
                            (offset < 0 && tick <= kCenterTick && tick > activeTick);
    const bool    active  = (tick == activeTick);

    heights[i] = (between || active) ? 6u : 4u;
    if (active) {
      colors[i] = (offset == 0) ? Color::kLock : Color::kWatts;
    } else {
      colors[i] = between ? Color::kAdv : Color::kTrack;
    }
  }

  drawSegmentBar(kVernierX, kVernierBaseY, kVernierW, kVernierCount, heights, colors, 80,
                 Color::kInk, -1, Color::kInk, 12);
}

} // namespace

void drawActiveVinCalibrate(uint16_t voltageX10, uint16_t divisor) {
  Display::clearScreenColor();
  drawSolidRail(Color::kAdv);
  Display::drawSpaceGroteskRgb("VIN trim", 10, 5, 15, Color::kInk);
  drawVoltagePill(voltageX10);

  drawVernier(clampedDivisorOffset(divisor));
  drawChevrons(Color::kAdv);

  char divisorText[12];
  appendUnsigned(divisorText, divisor);
  Display::drawSpaceGroteskRgb(divisorText, 26, 49, 19, Color::kInk);
  drawPill(67, 50, 68, 20, Color::kLockDim, Color::kLock, "OK Save", Color::kLock);
}

} // namespace ChaosHud

#endif // LCD_160x80
