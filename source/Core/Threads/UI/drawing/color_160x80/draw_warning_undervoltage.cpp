/*
 * draw_warning_undervoltage.cpp — Undervoltage lockout (UVLO) alert screen for HS-02.
 *
 * Purpose:
 *   Renders the blocking UVLO fault screen showing live measured input voltage, active
 *   pack/DC cutoff threshold pill, and heater cutoff state.
 *
 * Why it exists:
 *   Implements ui_draw_warning_undervoltage() for LCD_160x80 using HudTelemetry.
 */

#include "HudTelemetry.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

namespace {

void drawCutoffPill() {
  char           cutoff[16];
  const uint32_t cells = getSettingValue(SettingsOptions::MinDCVoltageCells);
  if (cells > 0u) {
    const uint32_t minCellX10 = getSettingValue(SettingsOptions::MinVoltageCells);
    ChaosHud::appendStr(cutoff, "<");
    ChaosHud::appendTenths(cutoff + 1, (2u + cells) * minCellX10, "V");
  } else {
    ChaosHud::appendStr(cutoff, "< 6.0V");
  }

  ChaosHud::drawPill(10, 55, 58, 19, ChaosHud::Color::kRedlineDim, ChaosHud::Color::kNone, cutoff,
                     ChaosHud::Color::kRedline);
}

} // namespace

void ui_draw_warning_undervoltage(void) {
  Display::clearScreenColor();
  ChaosHud::drawSolidRail(ChaosHud::Color::kWatts);
  ChaosHud::drawHazardStripe(ChaosHud::Color::kWatts, ChaosHud::Color::kWattsDim);

  Display::drawAaBitmapRgb(Hs02UI::kIconTriangleAlert.data, Hs02UI::kIconTriangleAlert.width,
                           Hs02UI::kIconTriangleAlert.height, 10, 8, ChaosHud::Color::kWatts);
  Display::drawSpaceGroteskRgb("Undervoltage", 32, 7, 15, ChaosHud::Color::kInk);

  char  voltageText[24];
  char *voltageEnd = ChaosHud::appendStr(voltageText, "Input ");
  ChaosHud::formatLiveVin(voltageEnd, " V");
  Display::drawSpaceGroteskRgb(voltageText, 10, 29, 19, ChaosHud::Color::kWatts);

  drawCutoffPill();
  ChaosHud::drawPill(72, 55, 40, 19, ChaosHud::Color::kWattsDim, ChaosHud::Color::kNone, "CUT",
                     ChaosHud::Color::kWatts);
  ChaosHud::drawPill(116, 55, 36, 19, ChaosHud::Color::kCell, ChaosHud::Color::kBorder, "OK",
                     ChaosHud::Color::kInk);

  Display::refresh();
  GUIDelay();
  waitForButtonPress();
}

#endif // LCD_160x80
