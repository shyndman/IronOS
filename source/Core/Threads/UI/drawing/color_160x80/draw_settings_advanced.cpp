/*
 * draw_settings_advanced.cpp — Advanced category setting screens & help-pill formatter.
 *
 * Purpose:
 *   Renders the seven Advanced sub-menu items (Power limit, Calibrate CJC at next boot,
 *   Calibrate input voltage, Power pulse, Power pulse delay, Power pulse duration, and
 *   Restore default settings), plus the compact setting-value pill formatter shown in
 *   the top-right corner of the 4-second Settings help marquee view.
 *
 * Why it exists:
 *   Completes the 1-file-per-category Settings decomposition instead of mixing Advanced
 *   items with unrelated categories in arbitrary a/b files.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "BSP.h"
#include "TipThermoModel.h"
#include "configuration.h"

namespace ChaosHud {
namespace {

void drawPowerLimitSetting() {
  const uint16_t limitWatts = getSettingValue(SettingsOptions::PowerLimit);
  const uint8_t  activeCell = static_cast<uint8_t>((limitWatts + 9u) / 10u);
  uint16_t       colors[10];

  for (uint8_t i = 0; i < 10u; ++i) {
    if (limitWatts == 0u) {
      colors[i] = (i < 7u) ? Color::kAdv : Color::kWatts;
    } else if (i + 1u == activeCell) {
      colors[i] = Color::kWatts;
    } else if (i < activeCell) {
      colors[i] = Color::kAdv;
    } else if (i >= 7u) {
      colors[i] = Color::kRedlineDim;
    } else {
      colors[i] = Color::kTrack;
    }
  }

  drawUniformBar(Layout::kContentX, 31, Layout::kBarW, 7, 10, colors);

  char valBuf[12];
  if (limitWatts == 0u) {
    appendStr(valBuf, "Off");
  } else {
    char *p = appendUnsigned(valBuf, limitWatts);
    appendStr(p, " W");
  }

  char  vinBuf[12];
  char *vp = appendUnsigned(vinBuf, readLiveVinRoundedVolts());
  appendStr(vp, "V");

  drawSettingBottomRow(Color::kAdv, valBuf, vinBuf, Color::kVoltsDim, Color::kNone, Color::kVolts);
}

void drawCalibrateCjcSetting() {
  const uint32_t tipC       = TipThermoModel::getTipInC();
  const uint16_t handleCx10 = getHandleTemperature(0);
  char           pillBuf[16];

  if (tipC >= 50u) {
    char *p = appendStr(pillBuf, "Hot ");
    p       = appendUnsigned(p, tipC);
    appendStr(p, HS02_SYM_DEG);
    drawPillRight(152, 5, 19, Color::kRedlineDim, Color::kRedline, pillBuf, Color::kRedline, 4);
    drawTextRight("cool tip", 152, 25, 15, Color::kMuted);
  } else {
    const int32_t  deltaCx10 = static_cast<int32_t>(tipC * 10u) - static_cast<int32_t>(handleCx10);
    const uint32_t absCx10   = static_cast<uint32_t>(deltaCx10 < 0 ? -deltaCx10 : deltaCx10);
    appendTenths(pillBuf, absCx10, HS02_SYM_DEG);
    drawPillRight(152, 5, 19, Color::kWattsDim, Color::kWatts, pillBuf, Color::kWatts, 4);
    drawTextRight("drift", 152, 25, 15, Color::kMuted);
  }

  drawChevrons(Color::kAdv);
  static const char *const kLabels[2] = {"Off", "Armed"};
  const bool               armed      = getSettingValue(SettingsOptions::CalibrateCJC) > 0u;
  drawDetentStrip(26, 50, 109, 21, kLabels, 2, armed ? 1u : 0u,
                  armed ? Color::kAdv : Color::kCellHi, armed ? Color::kNone : Color::kBorder,
                  armed ? Color::kDarkInk : Color::kInk);
}

void drawVoltageCalibrationSetting() {
  char  divBuf[16];
  char *dp = appendStr(divBuf, "Div ");
  appendUnsigned(dp, getSettingValue(SettingsOptions::VoltageDiv));
  drawTextRight(divBuf, 152, 22, 15, Color::kMuted);

  drawPill(10, 50, 77, 21, Color::kAdvDim, Color::kAdv, "OK " HS02_SYM_DOT " Trim", Color::kAdv);

  char vinBuf[12];
  formatLiveVin(vinBuf);
  drawPill(91, 50, 45, 21, Color::kVoltsDim, Color::kNone, vinBuf, Color::kVolts);

  Display::drawAaBitmapRgb(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width,
                           Hs02UI::kIconChevronRight.height, Layout::kChevronRightX, 53,
                           Color::kAdv);
}

void drawPulsePowerSetting() {
  const uint16_t pulseX10 = getSettingValue(SettingsOptions::KeepAwakePulse);
  const uint16_t waitStep = getSettingValue(SettingsOptions::KeepAwakePulseWait);
  const uint8_t  litCells = static_cast<uint8_t>((pulseX10 + 9u) / 10u);
  uint16_t       colors[10];

  for (uint8_t i = 0; i < 10u; ++i) {
    if (i >= litCells) {
      colors[i] = Color::kTrack;
    } else if (i + 1u == litCells) {
      colors[i] = Color::kWatts;
    } else {
      colors[i] = Color::kAdv;
    }
  }
  drawUniformBar(Layout::kContentX, 31, Layout::kBarW, 6, 10, colors);

  char valBuf[16];
  if (pulseX10 == 0u) {
    appendStr(valBuf, "Off");
  } else {
    appendTenths(valBuf, pulseX10, " W");
  }

  char  delayBuf[12];
  char *p = appendUnsigned(delayBuf, (static_cast<uint32_t>(waitStep) * 25u) / 10u);
  appendStr(p, "s");

  drawSettingBottomRow(Color::kAdv, valBuf, delayBuf, Color::kVoltsDim, Color::kNone,
                       Color::kVolts);
}

void drawPulseWaitSetting() {
  static constexpr uint16_t kWaitColors[9] = {
      Color::kVolts, Color::kVolts, Color::kVolts, Color::kVolts, Color::kAdv,
      Color::kAdv,   Color::kAdv,   Color::kAdv,   Color::kAdv,
  };

  const uint16_t waitStep = getSettingValue(SettingsOptions::KeepAwakePulseWait);
  const uint16_t pulseX10 = getSettingValue(SettingsOptions::KeepAwakePulse);
  uint16_t       colors[9];

  for (uint8_t i = 0; i < 9u; ++i) {
    colors[i] = (i < waitStep) ? kWaitColors[i] : Color::kTrack;
  }
  drawUniformBar(Layout::kContentX, 31, Layout::kBarW, 6, 9, colors);

  char valBuf[16];
  appendTenths(valBuf, static_cast<uint32_t>(waitStep) * 25u, " s");

  char pulseBuf[12];
  appendTenths(pulseBuf, pulseX10, "W");

  drawSettingBottomRow(Color::kAdv, valBuf, pulseBuf, Color::kWattsDim, Color::kNone,
                       Color::kWatts);
}

void drawPulseDurationSetting() {
  const uint16_t durStep  = getSettingValue(SettingsOptions::KeepAwakePulseDuration);
  const uint16_t pulseX10 = getSettingValue(SettingsOptions::KeepAwakePulse);
  const uint16_t waitStep = getSettingValue(SettingsOptions::KeepAwakePulseWait);
  uint16_t       colors[9];

  for (uint8_t i = 0; i < 9u; ++i) {
    colors[i] = (i < durStep) ? Color::kAdv : Color::kTrack;
  }

  char pulseBuf[16];
  appendTenths(pulseBuf, pulseX10, " W");
  drawPill(104, 5, 48, 19, Color::kWattsDim, Color::kNone, pulseBuf, Color::kWatts);
  drawUniformBar(104, 29, 48, 6, 9, colors);

  char  valBuf[16];
  char *vp = appendUnsigned(valBuf, static_cast<uint32_t>(durStep) * 250u);
  appendStr(vp, " ms");

  char  delayBuf[12];
  char *dp = appendUnsigned(delayBuf, (static_cast<uint32_t>(waitStep) * 25u) / 10u);
  appendStr(dp, "s");

  drawSettingBottomRow(Color::kAdv, valBuf, delayBuf, Color::kVoltsDim, Color::kNone,
                       Color::kVolts);
}

void drawResetSetting() {
  Display::drawAaBitmapRgb(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width,
                           Hs02UI::kIconTriangleAlert.height, 132, 8, Color::kRedline);
  drawPill(10, 50, 122, 21, Color::kRedlineDim, Color::kRedline, "OK " HS02_SYM_DOT " Reset all",
           Color::kRedline);
  Display::drawAaBitmapRgb(Hs02UI::kIconChevronRight.data, Hs02UI::kIconChevronRight.width,
                           Hs02UI::kIconChevronRight.height, Layout::kChevronRightX, 53,
                           Color::kRedline);
}

void appendDegSummary(char *out, uint16_t value) {
  char *p = appendUnsigned(out, value);
  appendStr(p, tempUnitSuffix());
}

bool formatPowerAndSolderingSummary(SettingsItemIndex idx, char *out) {
  switch (idx) {
  case SettingsItemIndex::DCInCutoff: {
    const uint16_t cells = getSettingValue(SettingsOptions::MinDCVoltageCells);
    if (cells == 0u) {
      appendStr(out, "DC");
    } else {
      char *p = appendUnsigned(out, 2u + cells);
      appendStr(p, "S");
    }
    return true;
  }
  case SettingsItemIndex::MinVolCell:
    appendTenths(out, getSettingValue(SettingsOptions::MinVoltageCells), "V");
    return true;
  case SettingsItemIndex::QCMaxVoltage:
    appendTenths(out, getSettingValue(SettingsOptions::QCIdealVoltage), "V");
    return true;
  case SettingsItemIndex::BoostTemperature: {
    const uint16_t val = getSettingValue(SettingsOptions::BoostTemp);
    if (val == 0u) {
      appendStr(out, "Off");
    } else {
      appendDegSummary(out, val);
    }
    return true;
  }
  case SettingsItemIndex::AutoStart: {
    static const char *const kLabels[4] = {"Off", "Solder", "Sleep", "Room"};
    const uint16_t           mode       = getSettingValue(SettingsOptions::AutoStartMode);
    appendStr(out, kLabels[mode < 4u ? mode : 0u]);
    return true;
  }
  case SettingsItemIndex::TempChangeShortStep: {
    char *p = appendUnsigned(out, getSettingValue(SettingsOptions::TempChangeShortStep));
    appendStr(p, HS02_SYM_DEG);
    return true;
  }
  case SettingsItemIndex::TempChangeLongStep: {
    char *p = appendUnsigned(out, getSettingValue(SettingsOptions::TempChangeLongStep));
    appendStr(p, HS02_SYM_DEG);
    return true;
  }
  case SettingsItemIndex::LockingMode: {
    static const char *const kLabels[3] = {"Off", "Boost", "Full"};
    const uint16_t           mode       = getSettingValue(SettingsOptions::LockingMode);
    appendStr(out, kLabels[mode < 3u ? mode : 0u]);
    return true;
  }
  default:
    return false;
  }
}

bool formatSleepSummary(SettingsItemIndex idx, char *out) {
  switch (idx) {
  case SettingsItemIndex::MotionSensitivity: {
    const uint16_t val = getSettingValue(SettingsOptions::Sensitivity);
    if (val == 0u) {
      appendStr(out, "Off");
    } else {
      appendUnsigned(out, val);
    }
    return true;
  }
  case SettingsItemIndex::SleepTemperature:
    appendDegSummary(out, getSettingValue(SettingsOptions::SleepTemp));
    return true;
  case SettingsItemIndex::SleepTimeout: {
    const uint16_t val = getSettingValue(SettingsOptions::SleepTime);
    if (val == 0u) {
      appendStr(out, "Off");
    } else if (val < 6u) {
      char *p = appendUnsigned(out, val * 10u);
      appendStr(p, "s");
    } else {
      char *p = appendUnsigned(out, val - 5u);
      appendStr(p, "m");
    }
    return true;
  }
  case SettingsItemIndex::ShutdownTimeout: {
    const uint16_t val = getSettingValue(SettingsOptions::ShutdownTime);
    if (val == 0u) {
      appendStr(out, "Off");
    } else {
      char *p = appendUnsigned(out, val);
      appendStr(p, "m");
    }
    return true;
  }
  default:
    return false;
  }
}

bool formatUiSummary(SettingsItemIndex idx, char *out) {
  switch (idx) {
  case SettingsItemIndex::TemperatureUnit:
    appendStr(out, tempUnitSuffix());
    return true;
  case SettingsItemIndex::DisplayRotation: {
    static const char *const kLabels[3] = {"Right", "Left", "Auto"};
    const uint16_t           mode       = getSettingValue(SettingsOptions::OrientationMode);
    appendStr(out, kLabels[mode < 3u ? mode : 2u]);
    return true;
  }
  case SettingsItemIndex::CooldownBlink:
    appendStr(out, getSettingValue(SettingsOptions::CoolingTempBlink) ? "Strobe" : "Steady");
    return true;
  case SettingsItemIndex::ReverseButtonTempChange:
    appendStr(out, getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) ? "Swapped"
                                                                                    : "Normal");
    return true;
  case SettingsItemIndex::ReverseButtonSettings:
    appendStr(out, getSettingValue(SettingsOptions::ReverseButtonSettings) ? "Rev" : "Std");
    return true;
  default:
    return false;
  }
}

bool formatDisplaySummary(SettingsItemIndex idx, char *out) {
  switch (idx) {
  case SettingsItemIndex::Brightness: {
    const uint16_t level =
        getSettingValue(SettingsOptions::DisplayBrightness) / BRIGHTNESS_STEP + 1u;
    char *p = appendUnsigned(out, level);
    appendStr(p, "/9");
    return true;
  }
  case SettingsItemIndex::DisplayIdleTimeout: {
    char *p = appendUnsigned(out, getSettingValue(SettingsOptions::DisplayIdleTimeoutSeconds));
    appendStr(p, "s");
    return true;
  }
  case SettingsItemIndex::LOGOTime: {
    const uint16_t val = getSettingValue(SettingsOptions::LOGOTime);
    if (val == logoMode_t::SKIP) {
      appendStr(out, "Off");
    } else if (val == logoMode_t::ONETIME) {
      appendStr(out, "Once");
    } else if (val >= logoMode_t::INFINITY) {
      appendStr(out, "Inf");
    } else {
      char *p = appendUnsigned(out, val);
      appendStr(p, "s");
    }
    return true;
  }
  default:
    return false;
  }
}

void formatAdvancedOnlySummary(SettingsItemIndex idx, char *out) {
  switch (idx) {
  case SettingsItemIndex::PowerLimit: {
    const uint16_t val = getSettingValue(SettingsOptions::PowerLimit);
    if (val == 0u) {
      appendStr(out, "Off");
    } else {
      char *p = appendUnsigned(out, val);
      appendStr(p, " W");
    }
    break;
  }
  case SettingsItemIndex::CalibrateCJC:
    appendStr(out, getSettingValue(SettingsOptions::CalibrateCJC) ? "Armed" : "Off");
    break;
  case SettingsItemIndex::VoltageCalibration:
    appendUnsigned(out, getSettingValue(SettingsOptions::VoltageDiv));
    break;
  case SettingsItemIndex::PowerPulsePower: {
    const uint16_t val = getSettingValue(SettingsOptions::KeepAwakePulse);
    if (val == 0u) {
      appendStr(out, "Off");
    } else {
      appendTenths(out, val, " W");
    }
    break;
  }
  case SettingsItemIndex::PowerPulseWait:
    appendTenths(out,
                 static_cast<uint32_t>(getSettingValue(SettingsOptions::KeepAwakePulseWait)) * 25u,
                 " s");
    break;
  case SettingsItemIndex::PowerPulseDuration: {
    const uint32_t ms =
        static_cast<uint32_t>(getSettingValue(SettingsOptions::KeepAwakePulseDuration)) * 250u;
    char *p = appendUnsigned(out, ms);
    appendStr(p, " ms");
    break;
  }
  case SettingsItemIndex::SettingsReset:
    appendStr(out, "Reset");
    break;
  default:
    appendStr(out, "N/A");
    break;
  }
}

} // namespace

bool drawAdvancedSetting(SettingsItemIndex idx) {
  switch (idx) {
  case SettingsItemIndex::PowerLimit:
    drawPowerLimitSetting();
    return true;
  case SettingsItemIndex::CalibrateCJC:
    drawCalibrateCjcSetting();
    return true;
  case SettingsItemIndex::VoltageCalibration:
    drawVoltageCalibrationSetting();
    return true;
  case SettingsItemIndex::PowerPulsePower:
    drawPulsePowerSetting();
    return true;
  case SettingsItemIndex::PowerPulseWait:
    drawPulseWaitSetting();
    return true;
  case SettingsItemIndex::PowerPulseDuration:
    drawPulseDurationSetting();
    return true;
  case SettingsItemIndex::SettingsReset:
    drawResetSetting();
    return true;
  default:
    return false;
  }
}

void formatSettingSummaryPill(SettingsItemIndex idx, char *outBuf, size_t outSize) {
  if (!outBuf || outSize == 0u) {
    return;
  }

  char value[32] = {};
  if (!formatPowerAndSolderingSummary(idx, value) && !formatSleepSummary(idx, value) &&
      !formatUiSummary(idx, value) && !formatDisplaySummary(idx, value)) {
    formatAdvancedOnlySummary(idx, value);
  }

  size_t i = 0;
  while (value[i] != '\0' && i + 1u < outSize) {
    outBuf[i] = value[i];
    ++i;
  }
  outBuf[i] = '\0';
}

} // namespace ChaosHud

#endif // LCD_160x80
