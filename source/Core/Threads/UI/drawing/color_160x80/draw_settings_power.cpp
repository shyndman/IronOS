/*
 * draw_settings_power.cpp — Power category setting screens for HS-02.
 *
 * Purpose:
 *   Renders the Power sub-menu items (Power source / battery cell count, Minimum cell
 *   voltage with gate-clipped battery bar, and QC negotiated voltage tier).
 *
 * Why it exists:
 *   Isolates Power-supply configuration screens along the real Settings menu boundary.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "BSP.h"

namespace ChaosHud {
namespace {

constexpr int16_t kDetentY = 28;
constexpr int16_t kDetentH = 20;

void drawPowerSourceSetting() {
  static const char *const kLabels[5] = {"DC", "3S", "4S", "5S", "6S"};
  const uint16_t           cells      = getSettingValue(SettingsOptions::MinDCVoltageCells);
  const uint8_t            active     = (cells <= 4u) ? static_cast<uint8_t>(cells) : 0u;

  drawDetentStrip(Layout::kContentX, kDetentY, Layout::kBarW, kDetentH, kLabels, 5, active,
                  Color::kPwr, Color::kNone, Color::kDarkInk);

  char valBuf[14];
  if (cells == 0u) {
    appendStr(valBuf, "DC");
  } else {
    char *p = appendUnsigned(valBuf, 2u + cells);
    appendStr(p, " cells");
  }

  char vinBuf[12];
  formatLiveVin(vinBuf);
  drawSettingBottomRow(Color::kPwr, valBuf, vinBuf, Color::kVoltsDim, Color::kNone, Color::kVolts);
}

void drawMinVolCellSetting() {
  static constexpr uint16_t kCellPalette[10] = {
      Color::kRedline, Color::kRedline, Color::kRedline, Color::kRedline, Color::kWatts,
      Color::kLock,    Color::kLock,    Color::kLock,    Color::kLock,    Color::kVolts,
  };
  static constexpr uint16_t kUnlitPalette[10] = {
      Color::kTrack, Color::kTrack, Color::kTrack, Color::kTrack, Color::kTrack,
      Color::kTrack, Color::kTrack, Color::kTrack, Color::kTrack, Color::kTrack,
  };

  const uint16_t cellVx10 = getSettingValue(SettingsOptions::MinVoltageCells);
  const int16_t  gateX =
      mapRangeToBarX(static_cast<int32_t>(cellVx10), 24, 38, Layout::kContentX, Layout::kGateSpanW);

  drawSplitSegmentBar(Layout::kContentX, 37, Layout::kBarW, 10, gateX, 6, kCellPalette, 6,
                      kUnlitPalette, Color::kInk, 10);

  char valBuf[12];
  appendTenths(valBuf, cellVx10, "V");

  const uint16_t cellCount = 2u + getSettingValue(SettingsOptions::MinDCVoltageCells);
  const uint32_t packVx10  = static_cast<uint32_t>(cellCount) * cellVx10;
  char           packBuf[16];
  char          *p = appendUnsigned(packBuf, cellCount);
  p                = appendStr(p, "S ");
  appendTenths(p, packVx10, "V");

  drawSettingBottomRow(Color::kPwr, valBuf, packBuf, Color::kPwrDim, Color::kNone, Color::kPwr);
}

void drawQcVoltageSetting() {
  static const char *const kLabels[4] = {"9V", "12V", "15V", "20V"};
  const uint16_t           qcVx10     = getSettingValue(SettingsOptions::QCIdealVoltage);
  const uint8_t tier = (qcVx10 < 110u) ? 0u : ((qcVx10 < 140u) ? 1u : ((qcVx10 < 180u) ? 2u : 3u));

  drawDetentStrip(Layout::kContentX, kDetentY, Layout::kBarW, kDetentH, kLabels, 4, tier,
                  Color::kPwr, Color::kNone, Color::kDarkInk);

  char valBuf[12];
  appendTenths(valBuf, qcVx10, " V");

  const uint8_t  tipResX10 = getTipResistanceX10();
  const uint32_t denom     = 10u * (tipResX10 > 0u ? static_cast<uint32_t>(tipResX10) : 26u);
  uint32_t       watts     = (static_cast<uint32_t>(qcVx10) * qcVx10) / denom;
  if (watts > 100u) {
    watts = 100u;
  }

  char  wattBuf[10];
  char *p = appendUnsigned(wattBuf, watts);
  appendStr(p, "W");
  drawSettingBottomRow(Color::kPwr, valBuf, wattBuf, Color::kWattsDim, Color::kNone, Color::kWatts);
}

} // namespace

bool drawPowerSetting(SettingsItemIndex idx) {
  switch (idx) {
  case SettingsItemIndex::DCInCutoff:
    drawPowerSourceSetting();
    return true;
  case SettingsItemIndex::MinVolCell:
    drawMinVolCellSetting();
    return true;
  case SettingsItemIndex::QCMaxVoltage:
    drawQcVoltageSetting();
    return true;
  default:
    return false;
  }
}

} // namespace ChaosHud

#endif // LCD_160x80
