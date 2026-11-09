#include "Hs02UiAssets.hpp"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

char *appendUnsigned(char *p, uint32_t val) {
  char    rev[10];
  uint8_t n = 0;
  do {
    rev[n++] = static_cast<char>('0' + (val % 10));
    val /= 10;
  } while (val > 0);
  while (n > 0) {
    *p++ = rev[--n];
  }
  *p = '\0';
  return p;
}

char *appendStr(char *p, const char *s) {
  while (*s) {
    *p++ = *s++;
  }
  *p = '\0';
  return p;
}

char *appendTenthsUnit(char *p, uint32_t x10Val, char unit) {
  p    = appendUnsigned(p, x10Val / 10);
  *p++ = '.';
  *p++ = static_cast<char>('0' + (x10Val % 10));
  *p++ = ' ';
  *p++ = unit;
  *p   = '\0';
  return p;
}

void drawRightAligned(const char *str, int16_t rightX, int16_t y, uint8_t pxSize, uint8_t rampIndex) {
  const uint16_t w = Display::measureSpaceGrotesk(str, pxSize);
  Display::drawSpaceGrotesk(str, rightX - static_cast<int16_t>(w), y, pxSize, rampIndex);
}

void drawHeroTemperature(TemperatureType_t temp, uint8_t digitRamp) {
  char numBuf[12];
  appendUnsigned(numBuf, temp);
  const int16_t endX = Display::drawSpaceGrotesk(numBuf, 11, 4, 30, digitRamp);
  const char   *unit = getSettingValue(SettingsOptions::TemperatureInF) ? (HS02_SYM_DEG "F") : (HS02_SYM_DEG "C");
  Display::drawSpaceGrotesk(unit, endX + 2, 10, 17, 2);
}

void drawTargetLine(TemperatureType_t target, uint8_t rampIndex) {
  char buf[16];
  char *p = appendUnsigned(buf, target);
  appendStr(p, HS02_SYM_DEG);
  drawRightAligned(buf, 152, 25, 15, rampIndex);
}

void drawSetpointBar(TemperatureType_t targetInSettingUnits) {
  Display::drawRoundedBarColor(11, 47, 141, 4, 3);
  int32_t setpointC = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::convertFtoC(targetInSettingUnits) : targetInSettingUnits;
  if (setpointC < 100) {
    setpointC = 100;
  } else if (setpointC > 450) {
    setpointC = 450;
  }
  int32_t fillW = ((setpointC - 100) * 141) / (450 - 100);
  if (fillW < 4) {
    fillW = 4;
  } else if (fillW > 141) {
    fillW = 141;
  }
  Display::drawRoundedBarColor(11, 47, static_cast<uint8_t>(fillW), 4, 1);
}

} // namespace

void ui_draw_home_gauge_idle(TemperatureType_t tipTemp) {
  const uint32_t voltageX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char           vBuf[16];
  appendTenthsUnit(vBuf, voltageX10, 'V');

  if (isTipDisconnected()) {
    Display::setColorPalette(Hs02UI::kPaletteHomeCooling);
    Display::drawRoundedBarColor(0, 0, 3, 80, 1);
    Display::drawAaBitmapColor(Hs02UI::kIconUnlink.data, Hs02UI::kIconUnlink.width, Hs02UI::kIconUnlink.height, 11, 12, 1);
    Display::drawSpaceGrotesk("No tip", 40, 10, 19, 0);
    drawRightAligned("Open", 152, 7, 15, 1);
    drawRightAligned(HS02_SYM_DASH, 152, 25, 15, 2);
    Display::drawRoundedBarColor(11, 47, 141, 4, 3);
    Display::drawSpaceGrotesk(vBuf, 11, 56, 15, 2);
    drawRightAligned("Insert tip", 152, 56, 15, 1);
    return;
  }

  const bool              cooling     = tipTemp >= 50;
  const TemperatureType_t displayTemp = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : tipTemp;
  const TemperatureType_t target      = getSettingValue(SettingsOptions::SolderingTemp);

  Display::setColorPalette(cooling ? Hs02UI::kPaletteHomeCooling : Hs02UI::kPaletteHomeIdle);
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);
  drawHeroTemperature(displayTemp, cooling ? 1 : 0);
  drawRightAligned(cooling ? "Hot" : "Ready", 152, 7, 15, 1);
  drawTargetLine(target, 2);
  drawSetpointBar(target);
  Display::drawSpaceGrotesk(vBuf, 11, 56, 15, 2);
  drawRightAligned(cooling ? "Cooling" : "Standby", 152, 56, 15, cooling ? 1 : 2);
}

void ui_draw_home_gauge_soldering(bool boostModeOn) {
  Display::setColorPalette(boostModeOn ? Hs02UI::kPaletteHomeBoost : Hs02UI::kPaletteHomeSolder);
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);

  const TemperatureType_t current = getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC();
  const TemperatureType_t target  = getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp);

  drawHeroTemperature(current, 0);

  if (boostModeOn) {
    drawRightAligned("Boost", 152, 7, 15, 1);
  } else if (static_cast<int32_t>(target) - static_cast<int32_t>(current) > 10) {
    drawRightAligned("Heat", 152, 7, 15, 1);
  } else {
    drawRightAligned("Set", 152, 7, 15, 2);
  }

  drawTargetLine(target, 0);
  drawSetpointBar(target);

  char wBuf[16];
  appendTenthsUnit(wBuf, x10WattHistory.average(), 'W');
  Display::drawSpaceGrotesk(wBuf, 11, 56, 15, 1);

  const uint32_t voltageX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char           rightBuf[28];
  char          *p = appendTenthsUnit(rightBuf, voltageX10, 'V');
#ifndef NO_SLEEP_MODE
  if (getSettingValue(SettingsOptions::Sensitivity) && getSettingValue(SettingsOptions::SleepTime)) {
    const TickType_t lastEventTime = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
    const TickType_t timeout       = getSleepTimeout();
    const TickType_t elapsed       = xTaskGetTickCount() - lastEventTime;
    const TickType_t downCount     = (timeout > elapsed) ? (timeout - elapsed) : 0;
    const bool       minutes       = downCount >= TICKS_MIN;
    const uint32_t   amount        = minutes ? (downCount + TICKS_MIN - 1) / TICKS_MIN : (downCount / 1000 + 1);
    p                              = appendStr(p, " " HS02_SYM_DOT " ");
    p                              = appendUnsigned(p, amount);
    *p++                           = minutes ? 'm' : 's';
    *p                             = '\0';
  }
#endif
  drawRightAligned(rightBuf, 152, 56, 15, 2);
}

void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp) {
  Display::setColorPalette(Hs02UI::kPaletteHomeSleep);
  Display::drawRoundedBarColor(0, 0, 3, 80, 1);

  drawHeroTemperature(tipTemp, 0);
  drawRightAligned("Sleep", 152, 7, 15, 1);

  const TemperatureType_t sleepTarget = min(getSettingValue(SettingsOptions::SleepTemp), getSettingValue(SettingsOptions::SolderingTemp));
  drawTargetLine(sleepTarget, 2);
  drawSetpointBar(sleepTarget);

  char wBuf[16];
  appendTenthsUnit(wBuf, x10WattHistory.average(), 'W');
  Display::drawSpaceGrotesk(wBuf, 11, 56, 15, 1);

  const uint32_t voltageX10 = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char           vBuf[16];
  appendTenthsUnit(vBuf, voltageX10, 'V');
  drawRightAligned(vBuf, 152, 56, 15, 2);
}

#endif
