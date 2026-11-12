/*
 * HudTelemetry.cpp — Centralized HS-02 sensor, unit, and setting queries for the UI.
 *
 * Purpose:
 *   Implements shared temperature unit conversion, calibrated input voltage formatting,
 *   smoothed wattage lookup, CH224K / battery power-source tagging,button-polarity
 *   checks, and top-level setting screen dispatch across the five menu categories.
 *
 * Why it exists:
 *   Keeps hardware details (VoltageDiv scaling, x10WattHistory filter convention,
 *   CH224K negotiation state, and rotation XOR button inversion) in one module.
 */

#include "HudTelemetry.hpp"

#ifdef LCD_160x80

#include "BSP.h"
#include "Setup.h"
#include "TipThermoModel.h"
#include "power.hpp"

namespace ChaosHud {

bool isFahrenheit() { return getSettingValue(SettingsOptions::TemperatureInF) != 0u; }

const char *tempUnitSuffix() { return isFahrenheit() ? (HS02_SYM_DEG "F") : (HS02_SYM_DEG "C"); }

int32_t settingTempToC(TemperatureType_t settingTemp) {
  if (isFahrenheit()) {
    return static_cast<int32_t>(TipThermoModel::convertFtoC(settingTemp));
  }

  return static_cast<int32_t>(settingTemp);
}

uint16_t readLiveVinX10() {
  return getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
}

char *formatLiveVin(char *dst, const char *suffix) {
  return appendTenths(dst, readLiveVinX10(), suffix);
}

uint16_t readLiveVinRoundedVolts() { return static_cast<uint16_t>((readLiveVinX10() + 5u) / 10u); }

uint32_t readSmoothedWattsX10() {
  const int32_t avg = x10WattHistory.average();
  return (avg > 0) ? static_cast<uint32_t>(avg) : 0u;
}

char *formatPowerSourceTag(char *dst) {
  const uint16_t cells = getSettingValue(SettingsOptions::MinDCVoltageCells);
  if (cells > 0u) {
    char *p = appendUnsigned(dst, 2u + cells);
    return appendStr(p, "S");
  }

  if (CH224KVoltageSettled()) {
    const uint16_t qcVolts = getSettingValue(SettingsOptions::QCIdealVoltage) / 10u;
    char          *p       = appendUnsigned(dst, qcVolts);
    return appendStr(p, "V");
  }

  return appendStr(dst, "DC");
}

bool isTempButtonsReversed() {
  const bool swapped = getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0u;
  return Display::getRotation() ^ swapped;
}

bool isSettingsButtonsReversed() {
  const bool swapped = getSettingValue(SettingsOptions::ReverseButtonSettings) != 0u;
  return Display::getRotation() ^ swapped;
}

bool drawCustomSettingScreen(SettingsItemIndex idx) {
  return drawPowerSetting(idx) || drawSolderingSetting(idx) || drawSleepSetting(idx) ||
         drawUiSetting(idx) || drawAdvancedSetting(idx);
}

} // namespace ChaosHud

#endif // LCD_160x80
