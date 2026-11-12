/*
 * draw_settings_soldering.cpp — Soldering category setting screens for HS-02.
 *
 * Purpose:
 *   Renders the Soldering sub-menu items (Boost temperature, Start-up behavior,
 *   Short/Long temperature steps, and Button lock mode).
 *
 * Why it exists:
 *   Groups thermal setpoint and soldering ergonomics settings into their own module
 *   matching the Soldering menu category.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

namespace ChaosHud {
namespace {

constexpr int16_t kDetentY    = 28;
constexpr int16_t kDetentH    = 20;
constexpr int16_t kCompanionX = 112;
constexpr int16_t kCompanionY = 5;
constexpr int16_t kCompanionW = 41;
constexpr int16_t kCompanionH = 19;

void drawBoostTempSetting() {
  const uint16_t solderVal = getSettingValue(SettingsOptions::SolderingTemp);
  const uint16_t boostVal  = getSettingValue(SettingsOptions::BoostTemp);
  const int32_t  solderC   = settingTempToC(solderVal);
  const int32_t  boostC    = (boostVal > 0u) ? settingTempToC(boostVal) : 0;

  const int16_t solderGateX =
      mapRangeToBarX(solderC, 100, 450, Layout::kContentX, Layout::kGateSpanW);
  const int16_t boostRightX =
      (boostVal > 0u) ? mapRangeToBarX(boostC, 100, 450, Layout::kContentX, Layout::kGateSpanW)
                      : -1;

  uint16_t colors[12];
  for (uint8_t i = 0; i < 12u; ++i) {
    const int16_t segX     = segmentLeftX(Layout::kContentX, Layout::kBarW, 12, i);
    const int16_t segRight = static_cast<int16_t>(segX + segmentWidth(Layout::kBarW, 12));

    if (segRight <= solderGateX + 2) {
      colors[i] = (segRight >= solderGateX - 8) ? Color::kHeat : Color::kHeatDim;
    } else if (boostVal > 0u && segX <= boostRightX) {
      colors[i] = Color::kRedline;
    } else {
      colors[i] = Color::kTrack;
    }
  }

  drawUniformBar(Layout::kContentX, 31, Layout::kBarW, 6, 12, colors, solderGateX, Color::kInk);

  if (boostVal == 0u) {
    drawSettingBottomRow(Color::kHeat, "Off", "Disabled", Color::kCell, Color::kNone,
                         Color::kMuted);
    return;
  }

  char  valBuf[16];
  char *vp = appendUnsigned(valBuf, boostVal);
  vp       = appendStr(vp, " ");
  appendStr(vp, tempUnitSuffix());

  const int32_t delta = static_cast<int32_t>(boostVal) - static_cast<int32_t>(solderVal);
  char          deltaBuf[14];
  char         *dp = deltaBuf;
  if (delta >= 0) {
    *dp++ = '+';
    dp    = appendUnsigned(dp, static_cast<uint32_t>(delta));
  } else {
    dp = appendStr(dp, HS02_SYM_MINUS);
    dp = appendUnsigned(dp, static_cast<uint32_t>(-delta));
  }
  appendStr(dp, HS02_SYM_DEG);

  drawSettingBottomRow(Color::kHeat, valBuf, deltaBuf, Color::kRedlineDim, Color::kNone,
                       Color::kRedline);
}

void drawAutoStartSetting() {
  static const char *const  kDetents[4] = {"Off", "Hot", "Slp", "Idle"};
  static const char *const  kValues[4]  = {"Off", "Solder", "Sleep", "Room temp"};
  static constexpr uint16_t kFills[4]   = {Color::kMuted, Color::kHeat, Color::kSlp, Color::kVolts};

  const uint16_t rawMode = getSettingValue(SettingsOptions::AutoStartMode);
  const uint8_t  mode    = (rawMode <= 3u) ? static_cast<uint8_t>(rawMode) : 0u;

  drawDetentStrip(Layout::kContentX, kDetentY, Layout::kBarW, kDetentH, kDetents, 4, mode,
                  kFills[mode], Color::kNone, Color::kDarkInk);
  drawSettingBottomRow(Color::kHeat, kValues[mode]);
}

void drawTempStepSetting(bool isLongStep) {
  const uint16_t shortStep = getSettingValue(SettingsOptions::TempChangeShortStep);
  const uint16_t longStep  = getSettingValue(SettingsOptions::TempChangeLongStep);
  const uint16_t activeVal = isLongStep ? longStep : shortStep;
  const uint16_t otherVal  = isLongStep ? shortStep : longStep;
  const uint16_t themeFg   = isLongStep ? Color::kWatts : Color::kHeat;
  const uint16_t themeDim  = isLongStep ? Color::kWattsDim : Color::kHeatDim;

  char  compBuf[12];
  char *cp = appendStr(compBuf, isLongStep ? "T " : "H ");
  cp       = appendUnsigned(cp, otherVal);
  appendStr(cp, HS02_SYM_DEG);
  drawPill(kCompanionX, kCompanionY, kCompanionW, kCompanionH, Color::kCell, Color::kNone, compBuf,
           Color::kMuted);

  const uint16_t maxStep  = isLongStep ? 90u : 50u;
  const uint16_t litCount = 1u + (activeVal * 5u) / maxStep;
  uint16_t       pipColors[6];
  for (uint8_t i = 0; i < 6u; ++i) {
    pipColors[i] = (i < litCount) ? themeFg : Color::kTrack;
  }
  drawUniformBar(kCompanionX, 30, kCompanionW, 5, 6, pipColors);

  char  valBuf[12];
  char *vp = appendUnsigned(valBuf, activeVal);
  appendStr(vp, HS02_SYM_DEG);

  char  pillBuf[16];
  char *pp = appendStr(pillBuf, isLongStep ? "Hold " : ("Tap " HS02_SYM_PLUSMINUS));
  pp       = appendUnsigned(pp, activeVal);
  appendStr(pp, HS02_SYM_DEG);

  drawSettingBottomRow(Color::kHeat, valBuf, pillBuf, themeDim, Color::kNone, themeFg);
}

void drawLockingModeSetting() {
  static const char *const kLabels[3] = {"Off", "Boost", "Full"};
  const uint16_t           rawMode    = getSettingValue(SettingsOptions::LockingMode);
  const uint8_t            mode       = (rawMode <= 2u) ? static_cast<uint8_t>(rawMode) : 0u;

  const uint16_t activeFill = (mode == 0u) ? Color::kCellHi : Color::kHeat;
  const uint16_t activeText = (mode == 0u) ? Color::kInk : Color::kDarkInk;
  drawDetentStrip(Layout::kContentX, kDetentY, Layout::kBarW, kDetentH, kLabels, 3, mode,
                  activeFill, Color::kNone, activeText);

  if (mode == 0u) {
    drawSettingBottomRow(Color::kHeat, kLabels[mode], "Off", Color::kCell, Color::kNone,
                         Color::kMuted);
  } else {
    drawSettingBottomRow(Color::kHeat, kLabels[mode], "Armed", Color::kLockDim, Color::kNone,
                         Color::kLock);
  }
}

} // namespace

bool drawSolderingSetting(SettingsItemIndex idx) {
  switch (idx) {
  case SettingsItemIndex::BoostTemperature:
    drawBoostTempSetting();
    return true;
  case SettingsItemIndex::AutoStart:
    drawAutoStartSetting();
    return true;
  case SettingsItemIndex::TempChangeShortStep:
    drawTempStepSetting(false);
    return true;
  case SettingsItemIndex::TempChangeLongStep:
    drawTempStepSetting(true);
    return true;
  case SettingsItemIndex::LockingMode:
    drawLockingModeSetting();
    return true;
  default:
    return false;
  }
}

} // namespace ChaosHud

#endif // LCD_160x80
