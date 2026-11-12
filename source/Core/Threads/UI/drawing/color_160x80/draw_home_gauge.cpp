/*
 * draw_home_gauge.cpp — Primary operating HUD screens (Idle, Soldering, Boost, Sleep, No-tip).
 *
 * Purpose:
 *   Renders all seven live home/soldering operating states on the 160x80 color panel
 *   using a unified F1/FPV telemetry instrument layout (30px hero temperature, status
 *   latch badge, 12-segment thermal shift bar with target/boost gates, smoothed wattage,
 *   live input voltage, and sleep/shutdown countdowns).
 *
 * Why it exists:
 *   Replaces the legacy detailed/simplified/sleep monochrome home screens on LCD_160x80
 *   with one cohesive live telemetry module.
 */

#include "HudTelemetry.hpp"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

using namespace ChaosHud;

constexpr int16_t kHeroX        = 10;
constexpr int16_t kHeroY        = 2;
constexpr int16_t kUnitY        = 8;
constexpr int16_t kStatusPillY  = 5;
constexpr int16_t kStatusPillH  = 19;
constexpr int16_t kRightAlignX  = 152;
constexpr int16_t kTargetLineY  = 25;
constexpr int16_t kBarX         = 10;
constexpr int16_t kBarY         = 47;
constexpr int16_t kBarW         = 142;
constexpr int16_t kBarGateSpanW = 140;
constexpr int16_t kBarH         = 6;
constexpr uint8_t kBarSegCount  = 12;
constexpr int32_t kBarMinC      = 100;
constexpr int32_t kBarMaxC      = 450;
constexpr int32_t kBarSpanC     = kBarMaxC - kBarMinC;
constexpr int16_t kBottomTextY  = 58;
constexpr int16_t kWattsPipX    = 10;
constexpr int16_t kWattsPipY    = 62;
constexpr int16_t kWattsPipW    = 3;
constexpr int16_t kWattsPipH    = 9;
constexpr int16_t kWattsTextX   = 16;

void drawStatusPillRight(int16_t w, uint16_t fillRgb, uint16_t strokeRgb, const char *label,
                         uint16_t textRgb) {
  drawPill(static_cast<int16_t>(kRightAlignX - w), kStatusPillY, w, kStatusPillH, fillRgb,
           strokeRgb, label, textRgb);
}

void drawHeroTemperature(TemperatureType_t displayTemp, uint16_t heroColor) {
  char numBuf[12];
  appendUnsigned(numBuf, displayTemp);
  const int16_t endX = Display::drawSpaceGroteskRgb(numBuf, kHeroX, kHeroY, 30, heroColor);
  Display::drawSpaceGroteskRgb(tempUnitSuffix(), static_cast<int16_t>(endX + 2), kUnitY, 17,
                               Color::kMuted);
}

void drawTargetReadout(TemperatureType_t targetInSettingUnits, uint16_t textRgb) {
  char  buf[16];
  char *p = appendUnsigned(buf, targetInSettingUnits);
  appendStr(p, HS02_SYM_DEG);
  drawTextRight(buf, kRightAlignX, kTargetLineY, 15, textRgb);
}

void drawThermalShiftBar(int32_t tipInC, bool forceFirstLit, int32_t target1C, uint16_t gate1Rgb,
                         int32_t target2C = -1, uint16_t gate2Rgb = Color::kInk,
                         uint16_t overrideLitRgb = Color::kNone) {
  uint16_t colors[kBarSegCount];
  for (uint8_t i = 0; i < kBarSegCount; ++i) {
    const int32_t  segThresholdC = kBarMinC + (static_cast<int32_t>(i) * kBarSpanC) / kBarSegCount;
    const bool     lit = (i == 0 && (forceFirstLit || tipInC >= 20)) || (tipInC >= segThresholdC);
    const uint16_t litColor =
        (overrideLitRgb != Color::kNone) ? overrideLitRgb : thermalZoneColor(i, kBarSegCount);
    colors[i] = lit ? litColor : Color::kTrack;
  }

  const int16_t gate1X =
      (target1C >= 0) ? mapRangeToBarX(target1C, kBarMinC, kBarMaxC, kBarX, kBarGateSpanW) : -1;
  const int16_t gate2X =
      (target2C >= 0) ? mapRangeToBarX(target2C, kBarMinC, kBarMaxC, kBarX, kBarGateSpanW) : -1;
  drawUniformBar(kBarX, kBarY, kBarW, kBarH, kBarSegCount, colors, gate1X, gate1Rgb, gate2X,
                 gate2Rgb);
}

void drawNoTipHazardBar() {
  uint16_t colors[kBarSegCount];
  for (uint8_t i = 0; i < kBarSegCount; ++i) {
    colors[i] = ((i & 1u) == 0u) ? Color::kWattsDim : Color::kTrack;
  }
  drawUniformBar(kBarX, kBarY, kBarW, kBarH, kBarSegCount, colors);
}

void drawWattsAndVolts(int16_t rightOccupiedLeftX) {
  Display::drawBoxRgb(kWattsPipX, kWattsPipY, kWattsPipW, kWattsPipH, Color::kWatts, Color::kNone);

  char wBuf[16];
  appendTenths(wBuf, readSmoothedWattsX10(), " W");
  const int16_t wEndX =
      Display::drawSpaceGroteskRgb(wBuf, kWattsTextX, kBottomTextY, 15, Color::kWatts);

  char vBuf[16];
  formatLiveVin(vBuf, "V");
  const int16_t vWidth = static_cast<int16_t>(Display::measureSpaceGrotesk(vBuf, 15));
  int16_t       voltsX = static_cast<int16_t>(wEndX + ((rightOccupiedLeftX - wEndX - vWidth) / 2));
  if (voltsX < wEndX + 4) {
    voltsX = static_cast<int16_t>(wEndX + 4);
  }

  Display::drawSpaceGroteskRgb(vBuf, voltsX, kBottomTextY, 15, Color::kVolts, voltsX,
                               static_cast<int16_t>(rightOccupiedLeftX - 3));
}

void drawSolderingRightTelemetry(bool boostModeOn) {
  if (boostModeOn) {
    drawTextRight("MAX", kRightAlignX, kBottomTextY, 15, Color::kRedline);
    return;
  }

#ifndef NO_SLEEP_MODE
  if (getSettingValue(SettingsOptions::Sensitivity) &&
      getSettingValue(SettingsOptions::SleepTime)) {
    const TickType_t lastEventTime =
        (lastButtonTime < lastMovementTime) ? lastMovementTime : lastButtonTime;
    const TickType_t timeout   = getSleepTimeout();
    const TickType_t elapsed   = xTaskGetTickCount() - lastEventTime;
    const TickType_t downCount = (timeout > elapsed) ? (timeout - elapsed) : 0;
    const bool       inMinutes = downCount >= TICKS_MIN;
    const uint32_t   amount =
        inMinutes ? ((downCount + TICKS_MIN - 1) / TICKS_MIN) : ((downCount / 1000) + 1);

    char  tBuf[16];
    char *p = appendUnsigned(tBuf, amount);
    *p++    = inMinutes ? 'm' : 's';
    *p      = '\0';
    drawTextRight(tBuf, kRightAlignX, kBottomTextY, 15, Color::kMuted);
  }
#endif
}

} // namespace

void ui_draw_home_gauge_idle(TemperatureType_t tipTemp) {
  Display::clearScreenColor();

  char vBuf[16];
  formatLiveVin(vBuf, " V");

  if (isTipDisconnected()) {
    drawSolidRail(Color::kWatts);
    Display::drawAaBitmapRgb(Hs02UI::kIconUnlink.data, Hs02UI::kIconUnlink.width,
                             Hs02UI::kIconUnlink.height, 10, 10, Color::kWatts);
    Display::drawSpaceGroteskRgb("No tip", 36, 9, 19, Color::kInk);
    drawPill(102, kStatusPillY, 50, kStatusPillH, Color::kWattsDim, Color::kWatts, "Open",
             Color::kWatts);
    drawTextRight(HS02_SYM_DASH, kRightAlignX, kTargetLineY, 15, Color::kMuted);
    drawNoTipHazardBar();
    Display::drawSpaceGroteskRgb(vBuf, kHeroX, kBottomTextY, 15, Color::kVolts);
    drawTextRight("Insert tip", kRightAlignX, kBottomTextY, 15, Color::kWatts);
    return;
  }

  const bool              cooling     = tipTemp >= 50;
  const TemperatureType_t displayTemp = isFahrenheit() ? TipThermoModel::getTipInF() : tipTemp;
  const TemperatureType_t target      = getSettingValue(SettingsOptions::SolderingTemp);
  const int32_t           targetC     = settingTempToC(target);
  const bool     blinkDim  = cooling && getSettingValue(SettingsOptions::CoolingTempBlink) &&
                             (((xTaskGetTickCount() / (TICKS_SECOND / 4)) & 1u) != 0u);
  const uint16_t heroColor = cooling ? (blinkDim ? Color::kWattsDim : Color::kWatts) : Color::kInk;

  drawSolidRail(cooling ? Color::kWatts : Color::kVolts);
  drawHeroTemperature(displayTemp, heroColor);
  if (cooling) {
    drawStatusPillRight(44, Color::kWattsDim, Color::kWatts, "Hot", Color::kWatts);
  } else {
    drawStatusPillRight(54, Color::kVoltsDim, Color::kVolts, "Ready", Color::kVolts);
  }

  drawTargetReadout(target, Color::kMuted);
  drawThermalShiftBar(static_cast<int32_t>(tipTemp), false, targetC, Color::kInk, -1, Color::kInk,
                      cooling ? Color::kWatts : Color::kNone);
  Display::drawSpaceGroteskRgb(vBuf, kHeroX, kBottomTextY, 15, Color::kVolts);
  drawTextRight(cooling ? "Cooling" : "Standby", kRightAlignX, kBottomTextY, 15,
                cooling ? Color::kWatts : Color::kMuted);
}

void ui_draw_home_gauge_soldering(bool boostModeOn) {
  Display::clearScreenColor();

  const int32_t           tipInC = static_cast<int32_t>(TipThermoModel::getTipInC());
  const TemperatureType_t displayTemp =
      isFahrenheit() ? TipThermoModel::getTipInF() : static_cast<TemperatureType_t>(tipInC);
  const TemperatureType_t target =
      getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp);
  const int32_t targetC   = settingTempToC(target);
  const bool    heatingUp = !boostModeOn && ((targetC - tipInC) > 10);

  if (boostModeOn) {
    drawSolidRail(Color::kRedline);
    drawStatusPillRight(54, Color::kRedline, Color::kNone, "Boost", Color::kDarkInk);
    drawTargetReadout(target, Color::kWatts);
  } else if (heatingUp) {
    drawSolidRail(Color::kHeat);
    drawStatusPillRight(48, Color::kHeat, Color::kNone, "Heat", Color::kDarkInk);
    drawTargetReadout(target, Color::kInk);
  } else {
    drawSolidRail(Color::kLock);
    drawStatusPillRight(44, Color::kLock, Color::kNone, "Set", Color::kDarkInk);
    drawTargetReadout(target, Color::kLock);
  }

  drawHeroTemperature(displayTemp, Color::kInk);
  drawThermalShiftBar(tipInC, true, targetC, Color::kInk);
  const int16_t rightItemLeftX =
      boostModeOn ? static_cast<int16_t>(kRightAlignX - Display::measureSpaceGrotesk("MAX", 15))
                  : 122;
  drawWattsAndVolts(rightItemLeftX);
  drawSolderingRightTelemetry(boostModeOn);
}

void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp) {
  Display::clearScreenColor();

  const int32_t           tipInC        = static_cast<int32_t>(TipThermoModel::getTipInC());
  const TemperatureType_t solderSetting = getSettingValue(SettingsOptions::SolderingTemp);
  const TemperatureType_t sleepSetting =
      min(getSettingValue(SettingsOptions::SleepTemp), solderSetting);
  const int32_t sleepTargetC  = settingTempToC(sleepSetting);
  const int32_t solderTargetC = settingTempToC(solderSetting);

  drawSolidRail(Color::kSlp);
  drawHeroTemperature(tipTemp, Color::kInk);
  drawStatusPillRight(53, Color::kSlp, Color::kNone, "Sleep", Color::kDarkInk);
  drawTargetReadout(solderSetting, Color::kHeat);
  drawThermalShiftBar(tipInC, true, sleepTargetC, Color::kInk, solderTargetC, Color::kHeat,
                      Color::kSlp);

  int16_t        shutdownPillLeftX = kRightAlignX;
  const uint16_t shutdownMin       = getSettingValue(SettingsOptions::ShutdownTime);
  if (shutdownMin > 0 && lastMovementTime > 0) {
    const TickType_t totalTicks = static_cast<TickType_t>(shutdownMin) * TICKS_MIN;
    const TickType_t elapsed    = xTaskGetTickCount() - lastMovementTime;
    const TickType_t remTicks   = (totalTicks > elapsed) ? (totalTicks - elapsed) : 0;
    const uint32_t   remMin     = (remTicks + TICKS_MIN - 1) / TICKS_MIN;

    char  mBuf[16];
    char *p           = appendUnsigned(mBuf, remMin);
    *p++              = 'm';
    *p                = '\0';
    shutdownPillLeftX = drawPillRight(kRightAlignX, 56, 18, Color::kRedlineDim, Color::kNone, mBuf,
                                      Color::kRedline, 4);
  }

  drawWattsAndVolts(shutdownPillLeftX);
}

#endif // LCD_160x80
