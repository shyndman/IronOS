/*
 * draw_profile_advanced.cpp — Multi-phase reflow/soldering profile HUD for HS-02.
 *
 * Purpose:
 *   Renders OperatingMode::SolderingProfile in Chaos Telemetry style (live tip vs phase
 *   target temperature, phase badge, elapsed/goal timer, wattage, and input voltage)
 *   without relying on legacy 1bpp Display::print() calls.
 *
 * Why it exists:
 *   Satisfies the ui_draw_soldering_profile_advanced() contract from SolderingProfile.cpp
 *   cleanly on LCD_160x80 after removing the dead monochrome soldering status files.
 */

#include "HudTelemetry.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

using namespace ChaosHud;

char *appendClockMmSs(char *dst, uint32_t totalSeconds) {
  dst                 = appendUnsigned(dst, totalSeconds / 60u);
  *dst++              = ':';
  const uint32_t secs = totalSeconds % 60u;
  *dst++              = static_cast<char>('0' + (secs / 10u));
  *dst++              = static_cast<char>('0' + (secs % 10u));
  *dst                = '\0';
  return dst;
}

} // namespace

void drawProfileHeader(TemperatureType_t tipTemp, TemperatureType_t targetTemp, uint32_t phase,
                       uint16_t totalPhases, uint16_t accent) {
  char numBuf[12];
  appendUnsigned(numBuf, tipTemp);
  const int16_t endX = Display::drawSpaceGroteskRgb(numBuf, 10, 2, 30, Color::kInk);
  Display::drawSpaceGroteskRgb(tempUnitSuffix(), static_cast<int16_t>(endX + 2), 8, 17,
                               Color::kMuted);

  char       phaseBadge[14];
  const bool isPreheat  = (phase == 0u);
  const bool isCooldown = (phase > totalPhases);
  if (isPreheat) {
    appendStr(phaseBadge, "Pre");
  } else if (isCooldown) {
    appendStr(phaseBadge, "Cool");
  } else {
    char *p = appendStr(phaseBadge, "P");
    p       = appendUnsigned(p, phase);
    *p++    = '/';
    appendUnsigned(p, totalPhases);
  }
  drawPillRight(152, 5, 19, isCooldown ? Color::kWattsDim : Color::kHeatDim, accent, phaseBadge,
                accent);

  char  targetBuf[16];
  char *tp = appendUnsigned(targetBuf, targetTemp);
  appendStr(tp, HS02_SYM_DEG);
  drawTextRight(targetBuf, 152, 25, 15, Color::kInk);
}

void ui_draw_soldering_profile_advanced(TemperatureType_t tipTemp,
                                        TemperatureType_t profileCurrentTargetTemp,
                                        uint32_t phaseElapsedSeconds, uint32_t phase,
                                        const uint32_t phaseTimeGoal) {
  Display::clearScreenColor();

  const uint16_t totalPhases = getSettingValue(SettingsOptions::ProfilePhases);
  const bool     isPreheat   = (phase == 0u);
  const bool     isCooldown  = (phase > totalPhases);
  const uint16_t accent = isCooldown ? Color::kWatts : (isPreheat ? Color::kVolts : Color::kHeat);

  drawSolidRail(accent);
  drawProfileHeader(tipTemp, profileCurrentTargetTemp, phase, totalPhases, accent);

  const int32_t tipC    = settingTempToC(tipTemp);
  const int32_t targetC = settingTempToC(profileCurrentTargetTemp);
  uint16_t      barColors[12];
  for (uint8_t i = 0; i < 12u; ++i) {
    const int32_t thresholdC = 80 + (static_cast<int32_t>(i) * 320) / 12;
    barColors[i]             = (tipC >= thresholdC) ? thermalZoneColor(i, 12) : Color::kTrack;
  }
  const int16_t gateX = mapRangeToBarX(targetC, 80, 400, 10, 140);
  drawUniformBar(10, 47, 142, 6, 12, barColors, gateX, Color::kInk);

  char wBuf[16];
  appendTenths(wBuf, readSmoothedWattsX10(), " W");
  Display::drawSpaceGroteskRgb(wBuf, 10, 58, 15, Color::kWatts);

  if (!isPreheat && !isCooldown) {
    char  timerBuf[20];
    char *cp = appendClockMmSs(timerBuf, phaseElapsedSeconds);
    cp       = appendStr(cp, "/");
    appendClockMmSs(cp, phaseTimeGoal);
    drawTextRight(timerBuf, 152, 58, 15, Color::kMuted);
  } else {
    char vBuf[12];
    formatLiveVin(vBuf);
    drawTextRight(vBuf, 152, 58, 15, Color::kVolts);
  }
}

#endif // LCD_160x80
