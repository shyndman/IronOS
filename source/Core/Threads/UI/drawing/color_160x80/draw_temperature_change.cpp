/*
 * draw_temperature_change.cpp — Interactive setpoint adjustment HUD for HS-02.
 *
 * Purpose:
 *   Renders the "Set temp" screen entered when pressing +/- during idle or soldering,
 *   showing physical left/right button polarity paddles, 30px target setpoint, live
 *   tip temperature pill, and 12-segment thermal bar with target and boost gates.
 *
 * Why it exists:
 *   Dedicated screen renderer for OperatingMode::TemperatureAdjust.
 */

#include "HudTelemetry.hpp"
#include "TipThermoModel.h"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

using namespace ChaosHud;

constexpr int16_t kBarX        = 10;
constexpr int16_t kBarY        = 65;
constexpr int16_t kBarW        = 142;
constexpr int16_t kBarGateSpan = 140;
constexpr int16_t kBarH        = 6;
constexpr uint8_t kBarCount    = 12;
constexpr int32_t kTempMinC    = 100;
constexpr int32_t kTempMaxC    = 450;
constexpr int32_t kTempSpanC   = kTempMaxC - kTempMinC;

void drawLiveTipPill() {
  constexpr int16_t pillX = 96;
  constexpr int16_t pillY = 5;
  constexpr int16_t pillW = 56;
  constexpr int16_t pillH = 19;

  Display::drawBoxRgb(pillX, pillY, pillW, pillH, Color::kVoltsDim, Color::kNone, 4);
  Display::drawBoxRgb(103, 12, 5, 5, Color::kVolts, Color::kNone);

  const TemperatureType_t tip =
      isFahrenheit() ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC();
  char  buf[16];
  char *p = appendUnsigned(buf, tip);
  appendStr(p, HS02_SYM_DEG);
  Display::drawSpaceGroteskRgb(buf, 113, pillY, 15, Color::kVolts);
}

void drawKeyBadge(int16_t x, const char *label, uint16_t textColor) {
  constexpr int16_t badgeY = 31;
  constexpr int16_t badgeW = 24;
  constexpr int16_t badgeH = 22;

  Display::drawBoxRgb(x, badgeY, badgeW, badgeH, Color::kHeatDim, Color::kHeat, 4);
  drawTextCentered(label, x, badgeW, 30, 19, textColor);
}

void drawTargetHero(TemperatureType_t target) {
  char targetBuf[12];
  appendUnsigned(targetBuf, target);
  const char   *unit   = tempUnitSuffix();
  const int16_t numW   = static_cast<int16_t>(Display::measureSpaceGrotesk(targetBuf, 30));
  const int16_t unitW  = static_cast<int16_t>(Display::measureSpaceGrotesk(unit, 17));
  const int16_t totalW = static_cast<int16_t>(numW + 2 + unitW);
  const int16_t startX = static_cast<int16_t>(81 - totalW / 2);
  const int16_t endX   = Display::drawSpaceGroteskRgb(targetBuf, startX, 24, 30, Color::kInk);

  Display::drawSpaceGroteskRgb(unit, static_cast<int16_t>(endX + 2), 30, 17, Color::kMuted);
}

void drawTemperatureBar(int32_t tipC, int32_t targetC, int32_t boostC) {
  uint16_t colors[kBarCount];
  for (uint8_t i = 0; i < kBarCount; ++i) {
    const int32_t segThresholdC = kTempMinC + (static_cast<int32_t>(i) * kTempSpanC) / kBarCount;
    if (tipC >= segThresholdC) {
      colors[i] = Color::kVolts;
    } else if (targetC >= segThresholdC) {
      colors[i] = Color::kHeat;
    } else {
      colors[i] = Color::kTrack;
    }
  }

  const int16_t targetGate = mapRangeToBarX(targetC, kTempMinC, kTempMaxC, kBarX, kBarGateSpan);
  const int16_t boostGate =
      (boostC > 0) ? mapRangeToBarX(boostC, kTempMinC, kTempMaxC, kBarX, kBarGateSpan) : -1;
  drawUniformBar(kBarX, kBarY, kBarW, kBarH, kBarCount, colors, targetGate, Color::kInk, boostGate,
                 Color::kRedline);
}

} // namespace

void ui_draw_temperature_change(void) {
  Display::clearScreenColor();
  drawSolidRail(Color::kHeat);
  Display::drawSpaceGroteskRgb("Set temp", 10, 5, 15, Color::kInk);
  drawLiveTipPill();

  const bool plusOnLeft = isTempButtonsReversed();
  drawKeyBadge(10, plusOnLeft ? "+" : HS02_SYM_MINUS, Color::kHeat);
  drawKeyBadge(128, plusOnLeft ? HS02_SYM_MINUS : "+", Color::kHeat);
  drawTargetHero(getSettingValue(SettingsOptions::SolderingTemp));

  const int32_t  tipC     = static_cast<int32_t>(TipThermoModel::getTipInC());
  const int32_t  targetC  = settingTempToC(getSettingValue(SettingsOptions::SolderingTemp));
  const uint16_t boostRaw = getSettingValue(SettingsOptions::BoostTemp);
  const int32_t  boostC   = (boostRaw > 0u) ? settingTempToC(boostRaw) : 0;
  drawTemperatureBar(tipC, targetC, boostC);
}

#endif // LCD_160x80
