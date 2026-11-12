/*
 * HudTelemetry.hpp — Centralized HS-02 sensor, unit, and setting queries for the UI.
 *
 * Purpose:
 *   Single home for every live hardware read, unit conversion, and button-polarity
 *   query used across Chaos Telemetry screens, plus the cross-file screen entrypoints
 *   invoked by SettingsMenu.cpp and settingsGUI.cpp.
 *
 * Why it exists:
 *   Eliminates copy-pasted settingTempToC(), getInputVoltageX10(), CH224K power-source
 *   classification, and rotation-XOR button polarity checks from individual screen files.
 */

#pragma once

#include "HudWidgets.hpp"
#include "Settings.h"
#include "Types.h"
#include "settingsGUI.hpp"
#include <stddef.h>
#include <stdint.h>

#ifdef LCD_160x80

namespace ChaosHud {

bool        isFahrenheit();
const char *tempUnitSuffix();
int32_t     settingTempToC(TemperatureType_t settingTemp);

uint16_t readLiveVinX10();
char    *formatLiveVin(char *dst, const char *suffix = "V");
uint16_t readLiveVinRoundedVolts();
uint32_t readSmoothedWattsX10();
char    *formatPowerSourceTag(char *dst);

bool isTempButtonsReversed();
bool isSettingsButtonsReversed();

// Top-level Chaos Telemetry screen entrypoints invoked by UI logic / settingsGUI.
void drawCategoryMenu(size_t categoryIndex);
void drawSettingSubScreen(SettingsItemIndex idx, uint8_t mainEntry, uint8_t menuLen, uint8_t visIdx,
                          bool showHelp, TickType_t elapsedSinceButton);
TickType_t helpMarqueeMinTimeoutTicks(SettingsItemIndex idx);
bool       drawPowerSetting(SettingsItemIndex idx);
bool       drawSolderingSetting(SettingsItemIndex idx);
bool       drawSleepSetting(SettingsItemIndex idx);
bool       drawUiSetting(SettingsItemIndex idx);
bool       drawAdvancedSetting(SettingsItemIndex idx);
bool       drawCustomSettingScreen(SettingsItemIndex idx);
void       formatSettingSummaryPill(SettingsItemIndex idx, char *outBuf, size_t outSize);
void       drawConfirmationModal(bool isResetSettings);
void       drawActiveVinCalibrate(uint16_t voltageX10, uint16_t divisor);

} // namespace ChaosHud

#endif // LCD_160x80
